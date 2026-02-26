/**
 * @file habitat_atmosphere.c
 * @brief Cabin atmosphere mass-balance and life-support logic.
 */

#include "habitat/include/habitat.h"
#include <math.h>
#include <stdlib.h>

#define MM_O2   0.032
#define MM_CO2  0.044
#define MM_N2   0.028
#define R_GAS   8.314

#define O2_TARGET_PCT   20.9
#define O2_THROTTLE_HI  21.5
#define O2_THROTTLE_LO  20.0

static double compute_total_moles(CabinAtmosphere *cab)
{
    return (cab->o2_mass_kg / MM_O2)
         + (cab->co2_mass_kg / MM_CO2)
         + (cab->n2_mass_kg / MM_N2);
}

static double mass_to_ppm(double gas_mass_kg, double mm, double n_total)
{
    if (n_total <= 0.0) return 0.0;
    return (gas_mass_kg / mm / n_total) * 1.0e6;
}

static double mass_to_pct(double gas_mass_kg, double mm, double n_total)
{
    if (n_total <= 0.0) return 0.0;
    return (gas_mass_kg / mm / n_total) * 100.0;
}

int habitat_atmosphere_update(Habitat *H)
{
    double dt = H->dt;
    CabinAtmosphere *cab = &H->cabin;

    double crew_factor = H->crew_count * H->crew_activity_factor;
    double o2_consumed  = O2_CONSUMPTION_KG_S  * crew_factor * dt;
    double co2_produced = CO2_PRODUCTION_KG_S  * crew_factor * dt;

    cab->o2_mass_kg  -= o2_consumed;
    cab->co2_mass_kg += co2_produced;
    if (cab->o2_mass_kg < 0.0) cab->o2_mass_kg = 0.0;

    double n_total = compute_total_moles(cab);
    double o2_pct_now = mass_to_pct(cab->o2_mass_kg, MM_O2, n_total);

    /* OGS with throttle control */
    OxygenGenSystem *ogs = &H->life_support.ogs;
    if (ogs->is_active && !ogs->has_fault) {
        double throttle = 1.0;
        if (o2_pct_now > O2_THROTTLE_HI) {
            throttle = 0.0;
        } else if (o2_pct_now > O2_THROTTLE_LO) {
            throttle = (O2_THROTTLE_HI - o2_pct_now)
                       / (O2_THROTTLE_HI - O2_THROTTLE_LO);
        }
        ogs->current_output_kg_s = ogs->max_production_kg_s * throttle;
        cab->o2_mass_kg += ogs->current_output_kg_s * dt;
        ogs->runtime_hours += dt / 3600.0;
    } else {
        ogs->current_output_kg_s = 0.0;
    }

    /* SBAR CO2 scrubber */
    CO2Scrubber *sbar = &H->life_support.sbar;
    if (sbar->is_active && !sbar->has_fault) {
        sbar->sorbent_efficiency -= sbar->degradation_rate * dt;
        if (sbar->sorbent_efficiency < 0.5)
            sbar->sorbent_efficiency = 0.5;

        sbar->cycle_timer_s += dt;
        if (sbar->cycle_timer_s >= sbar->cycle_period_s) {
            sbar->bed_selector = 1 - sbar->bed_selector;
            sbar->cycle_timer_s = 0.0;
        }

        sbar->current_removal_mol_s = sbar->max_removal_mol_s
                                      * sbar->sorbent_efficiency;
        double co2_removed_kg = sbar->current_removal_mol_s * MM_CO2 * dt;
        if (co2_removed_kg > cab->co2_mass_kg)
            co2_removed_kg = cab->co2_mass_kg;
        cab->co2_mass_kg -= co2_removed_kg;
    }

    /* Photothermal CO2 reactor */
    PhotothermalReactor *pt = &H->life_support.phototherm;
    if (pt->is_active && !pt->has_fault) {
        double roll = (double)rand() / (double)RAND_MAX;
        if (roll < pt->trip_probability) {
            pt->has_fault = 1;
            H->risk.phototherm_trip_count++;
        } else {
            double eff = 1.0 - (pt->catalyst_degradation
                         * H->mission_elapsed_time_s);
            if (eff < 0.5) eff = 0.5;

            double conversion = pt->conversion_mol_s * eff
                                * pt->capture_efficiency;
            double co2_converted_kg = conversion * MM_CO2 * dt;
            if (co2_converted_kg > cab->co2_mass_kg)
                co2_converted_kg = cab->co2_mass_kg;
            cab->co2_mass_kg -= co2_converted_kg;

            double o2_byproduct_kg = conversion * MM_O2 * 0.5 * dt;
            cab->o2_mass_kg += o2_byproduct_kg;

            pt->co_leakage_ppm = conversion * 0.001 * 1.0e6
                                 / (cab->cabin_volume_m3 * 40.0);
        }
    }

    /* Backup O2 supervisory logic */
    n_total = compute_total_moles(cab);
    o2_pct_now = mass_to_pct(cab->o2_mass_kg, MM_O2, n_total);

    BackupOxygen *bak = &H->life_support.backup_o2;
    if (o2_pct_now < O2_LOW_PCT) {
        if (bak->tank_mass_kg > 0.0) {
            double needed = O2_CONSUMPTION_KG_S * H->crew_count * dt * 1.5;
            if (needed > bak->tank_mass_kg) needed = bak->tank_mass_kg;
            cab->o2_mass_kg   += needed;
            bak->tank_mass_kg -= needed;
            bak->injection_rate_kg_s = needed / dt;
        } else if (bak->candles_remaining > 0 && !bak->candle_active) {
            bak->candle_active = 1;
            bak->candle_burn_elapsed = 0.0;
            bak->candles_remaining--;
        }
    } else {
        bak->injection_rate_kg_s = 0.0;
    }

    if (bak->candle_active) {
        cab->o2_mass_kg += bak->candle_rate_kg_s * dt;
        bak->candle_burn_elapsed += dt;
        if (bak->candle_burn_elapsed >= bak->candle_duration_s)
            bak->candle_active = 0;
    }

    double daily_need = O2_CONSUMPTION_KG_S * H->crew_count * 86400.0;
    bak->days_of_backup = (daily_need > 0.0)
                          ? bak->tank_mass_kg / daily_need : 0.0;

    /* Recompute concentrations */
    n_total = compute_total_moles(cab);
    cab->o2_pct  = mass_to_pct(cab->o2_mass_kg, MM_O2, n_total);
    cab->co2_ppm = mass_to_ppm(cab->co2_mass_kg, MM_CO2, n_total);
    cab->n2_pct  = mass_to_pct(cab->n2_mass_kg, MM_N2, n_total);

    if (cab->o2_mass_kg < 0.0)  cab->o2_mass_kg = 0.0;
    if (cab->co2_mass_kg < 0.0) cab->co2_mass_kg = 0.0;
    if (cab->o2_pct < 0.0)     cab->o2_pct = 0.0;
    if (cab->co2_ppm < 0.0)    cab->co2_ppm = 0.0;

    return 0;
}

int habitat_life_support_update(Habitat *H)
{
    LifeSupport *ls = &H->life_support;
    ls->total_power_w = 0.0;
    if (ls->ogs.is_active && !ls->ogs.has_fault)
        ls->total_power_w += ls->ogs.power_draw_w;
    if (ls->sbar.is_active && !ls->sbar.has_fault)
        ls->total_power_w += ls->sbar.power_draw_w;
    if (ls->phototherm.is_active && !ls->phototherm.has_fault)
        ls->total_power_w += ls->phototherm.power_draw_w;
    return 0;
}
