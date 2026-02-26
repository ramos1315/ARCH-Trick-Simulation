/**
 * @file habitat_atmosphere.c
 * @brief Cabin atmosphere mass-balance and life-support logic.
 *
 * Implements time-stepped updates for:
 *   - CO₂ / O₂ mass balance (crew metabolic loads vs. removal/supply)
 *   - OGS electrolysis O₂ production
 *   - SBAR sorbent CO₂ scrubbing with dual-bed cycling
 *   - Photothermal CO₂ conversion with degradation & random trips
 *   - Backup O₂ (compressed tanks + oxygen candles) supervisory logic
 */

#include "../include/habitat.h"
#include <math.h>
#include <stdlib.h>

/* Molar masses */
#define MM_O2   0.032
#define MM_CO2  0.044
#define R_GAS   8.314

/* ------------------------------------------------------------------ */
/*  Helper: convert mass ↔ concentration                               */
/* ------------------------------------------------------------------ */
static double mass_to_ppm(double gas_mass_kg, double molar_mass,
                          double total_moles)
{
    double gas_moles = gas_mass_kg / molar_mass;
    return (gas_moles / total_moles) * 1.0e6;
}

static double mass_to_pct(double gas_mass_kg, double molar_mass,
                          double total_moles)
{
    double gas_moles = gas_mass_kg / molar_mass;
    return (gas_moles / total_moles) * 100.0;
}

/* ------------------------------------------------------------------ */
/*  Cabin atmosphere update                                            */
/* ------------------------------------------------------------------ */
int habitat_atmosphere_update(Habitat *H)
{
    double dt = H->dt;
    CabinAtmosphere *cab = &H->cabin;

    /* --- Crew metabolic loads ------------------------------------ */
    double crew_factor = H->crew_count * H->crew_activity_factor;
    double o2_consumed  = O2_CONSUMPTION_KG_S  * crew_factor * dt;
    double co2_produced = CO2_PRODUCTION_KG_S  * crew_factor * dt;

    cab->o2_mass_kg  -= o2_consumed;
    cab->co2_mass_kg += co2_produced;

    /* --- O₂ supply from OGS ------------------------------------- */
    OxygenGenSystem *ogs = &H->life_support.ogs;
    if (ogs->is_active && !ogs->has_fault) {
        double o2_added = ogs->current_output_kg_s * dt;
        cab->o2_mass_kg += o2_added;
        ogs->runtime_hours += dt / 3600.0;
    }

    /* --- CO₂ removal: SBAR scrubber ------------------------------ */
    CO2Scrubber *sbar = &H->life_support.sbar;
    if (sbar->is_active && !sbar->has_fault) {
        /* Sorbent degradation */
        sbar->sorbent_efficiency -= sbar->degradation_rate * dt;
        if (sbar->sorbent_efficiency < 0.1)
            sbar->sorbent_efficiency = 0.1;

        /* Dual-bed cycling */
        sbar->cycle_timer_s += dt;
        if (sbar->cycle_timer_s >= sbar->cycle_period_s) {
            sbar->bed_selector = 1 - sbar->bed_selector;
            sbar->cycle_timer_s = 0.0;
        }

        sbar->current_removal_mol_s = sbar->max_removal_mol_s
                                      * sbar->sorbent_efficiency;
        double co2_removed_kg = sbar->current_removal_mol_s * MM_CO2 * dt;

        /* Cannot remove more than exists */
        if (co2_removed_kg > cab->co2_mass_kg)
            co2_removed_kg = cab->co2_mass_kg;

        cab->co2_mass_kg -= co2_removed_kg;
    }

    /* --- CO₂ conversion: photothermal reactor -------------------- */
    PhotothermalReactor *pt = &H->life_support.phototherm;
    if (pt->is_active && !pt->has_fault) {
        /* Random trip check */
        double roll = (double)rand() / (double)RAND_MAX;
        if (roll < pt->trip_probability) {
            pt->has_fault = 1;  /* tripped – will need manual reset */
            H->risk.phototherm_trip_count++;
        } else {
            /* Catalyst degradation */
            double eff = 1.0 - (pt->catalyst_degradation
                         * H->mission_elapsed_time_s);
            if (eff < 0.3) eff = 0.3;

            double conversion = pt->conversion_mol_s * eff
                                * pt->capture_efficiency;
            double co2_converted_kg = conversion * MM_CO2 * dt;

            if (co2_converted_kg > cab->co2_mass_kg)
                co2_converted_kg = cab->co2_mass_kg;

            cab->co2_mass_kg -= co2_converted_kg;

            /* O₂ byproduct: CO₂ + H₂O → CO + H₂ + O₂ (simplified) */
            double o2_byproduct_kg = conversion * MM_O2 * 0.5 * dt;
            cab->o2_mass_kg += o2_byproduct_kg;

            /* CO leakage (trace) */
            pt->co_leakage_ppm = conversion * 0.001 * 1.0e6
                                 / (cab->cabin_volume_m3 * 40.0);
        }
    }

    /* --- Backup O₂ supervisory logic ----------------------------- */
    /* Compute current O₂ percentage to decide if backup needed */
    double n_total = (cab->total_pressure_kpa * 1000.0 * cab->cabin_volume_m3)
                     / (R_GAS * cab->temperature_k);
    double o2_pct_now = mass_to_pct(cab->o2_mass_kg, MM_O2, n_total);

    BackupOxygen *bak = &H->life_support.backup_o2;

    if (o2_pct_now < O2_LOW_PCT) {
        /* Primary OGS failed or insufficient – activate backup */
        if (bak->tank_mass_kg > 0.0) {
            /* Inject from compressed tank */
            double needed = O2_CONSUMPTION_KG_S * H->crew_count * dt * 1.5;
            if (needed > bak->tank_mass_kg)
                needed = bak->tank_mass_kg;
            cab->o2_mass_kg    += needed;
            bak->tank_mass_kg  -= needed;
            bak->injection_rate_kg_s = needed / dt;
        } else if (bak->candles_remaining > 0 && !bak->candle_active) {
            /* Light an oxygen candle */
            bak->candle_active       = 1;
            bak->candle_burn_elapsed = 0.0;
            bak->candles_remaining--;
        }
    } else {
        bak->injection_rate_kg_s = 0.0;
    }

    /* Oxygen candle burn logic */
    if (bak->candle_active) {
        double o2_from_candle = bak->candle_rate_kg_s * dt;
        cab->o2_mass_kg += o2_from_candle;
        bak->candle_burn_elapsed += dt;
        if (bak->candle_burn_elapsed >= bak->candle_duration_s) {
            bak->candle_active = 0;
        }
    }

    /* Update backup days estimate */
    double daily_need = O2_CONSUMPTION_KG_S * H->crew_count * 86400.0;
    bak->days_of_backup = (daily_need > 0.0)
                          ? bak->tank_mass_kg / daily_need
                          : 0.0;

    /* --- Recompute concentrations -------------------------------- */
    n_total = (cab->total_pressure_kpa * 1000.0 * cab->cabin_volume_m3)
              / (R_GAS * cab->temperature_k);

    cab->o2_pct  = mass_to_pct(cab->o2_mass_kg, MM_O2, n_total);
    cab->co2_ppm = mass_to_ppm(cab->co2_mass_kg, MM_CO2, n_total);
    cab->n2_pct  = mass_to_pct(cab->n2_mass_kg, 0.028, n_total);

    /* Clamp physical limits */
    if (cab->o2_pct < 0.0)   cab->o2_pct  = 0.0;
    if (cab->co2_ppm < 0.0)  cab->co2_ppm = 0.0;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Life support aggregated update                                     */
/* ------------------------------------------------------------------ */
int habitat_life_support_update(Habitat *H)
{
    LifeSupport *ls = &H->life_support;

    /* Sum power draws */
    ls->total_power_w = 0.0;
    if (ls->ogs.is_active && !ls->ogs.has_fault)
        ls->total_power_w += ls->ogs.power_draw_w;
    if (ls->sbar.is_active && !ls->sbar.has_fault)
        ls->total_power_w += ls->sbar.power_draw_w;
    if (ls->phototherm.is_active && !ls->phototherm.has_fault)
        ls->total_power_w += ls->phototherm.power_draw_w;

    return 0;
}
