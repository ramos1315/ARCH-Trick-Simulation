# ============================================================
#  fix_physics.ps1
#
#  Fixes the simulation physics:
#    1. CO2: total n_moles was computed wrong (used pressure-based
#       ideal gas instead of actual tracked gas masses), causing
#       ppm to explode. Now uses sum of tracked gas moles.
#    2. O2: OGS had no throttle, produced O2 endlessly.
#       Now throttles between 20-21.5% O2.
#    3. Thermal: air-only thermal mass was too small, cabin
#       cooled instantly. Now includes structure + equipment
#       mass. Radiator is controlled, regolith insulation
#       properly modeled. Proportional heater control.
#    4. Init: photothermal degradation rate was too aggressive.
#
#  Run from: C:\Users\bnard\Documents\files
# ============================================================

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "Fixing simulation physics..." -ForegroundColor Cyan
Write-Host ""

# ── Fix 1: Replace habitat_atmosphere.c ──────────────────────

Write-Host "[1/3] Fixing atmosphere model (CO2 + O2 control)..." -ForegroundColor Yellow

$atmoContent = @'
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
'@

Set-Content -Path "models\habitat\src\habitat_atmosphere.c" -Value $atmoContent
Write-Host "  habitat_atmosphere.c replaced." -ForegroundColor Green

# ── Fix 2: Replace habitat_thermal.c ─────────────────────────

Write-Host "[2/3] Fixing thermal model (mass + heater control)..." -ForegroundColor Yellow

$thermalContent = @'
/**
 * @file habitat_thermal.c
 * @brief Thermal control with proper thermal mass and heater control.
 */

#include "habitat/include/habitat.h"
#include <math.h>

#define CP_AIR          1005.0
#define RHO_AIR         1.225
#define CP_STRUCTURE    500.0
#define STRUCTURE_MASS  5000.0
#define EQUIPMENT_MASS  3000.0
#define CP_EQUIPMENT    900.0
#define SIGMA           5.67e-8
#define EMISSIVITY      0.85
#define HEATER_MAX_W    8000.0

int habitat_thermal_update(Habitat *H)
{
    double dt = H->dt;
    ThermalControl *th = &H->thermal;

    th->metabolic_heat_w = H->crew_count * H->crew_activity_factor
                           * METABOLIC_HEAT_W;
    double q_in = th->metabolic_heat_w + th->equipment_heat_w
                + th->heater_output_w;

    /* Controlled radiator: only reject when above setpoint */
    if (th->pump_active && th->cabin_temp_k > CABIN_TEMP_NOM_K) {
        double T_rad = th->coolant_temp_k;
        double max_rej = EMISSIVITY * SIGMA * th->radiator_area_m2
                         * (pow(T_rad, 4.0) - pow(th->exterior_temp_k, 4.0));
        if (max_rej < 0.0) max_rej = 0.0;
        double frac = (th->cabin_temp_k - CABIN_TEMP_NOM_K) / 5.0;
        if (frac > 1.0) frac = 1.0;
        if (frac < 0.0) frac = 0.0;
        th->radiator_rejection_w = max_rej * frac;
    } else {
        th->radiator_rejection_w = 0.0;
    }

    /* Conduction through regolith - heavily attenuated */
    double A_shell = 4.0 * M_PI * 4.0 * 4.0;
    double q_cond = A_shell * (th->cabin_temp_k - th->exterior_temp_k)
                    / th->insulation_r_value;
    q_cond *= 0.15;  /* regolith thermal buffer */

    double q_out = th->radiator_rejection_w + q_cond;

    /* Thermal mass: air + structure + equipment */
    double m_air = RHO_AIR * H->cabin.cabin_volume_m3;
    double C_total = (m_air * CP_AIR)
                   + (STRUCTURE_MASS * CP_STRUCTURE)
                   + (EQUIPMENT_MASS * CP_EQUIPMENT);

    th->cabin_temp_k += (q_in - q_out) / C_total * dt;
    H->cabin.temperature_k = th->cabin_temp_k;

    /* Proportional heater control */
    double temp_error = CABIN_TEMP_NOM_K - th->cabin_temp_k;
    if (temp_error > 0.0) {
        double frac = temp_error / 5.0;
        if (frac > 1.0) frac = 1.0;
        th->heater_output_w     = HEATER_MAX_W * frac;
        th->heater_power_draw_w = th->heater_output_w;
    } else {
        th->heater_output_w     = 0.0;
        th->heater_power_draw_w = 0.0;
    }

    double tau_coolant = 300.0;
    th->coolant_temp_k += (th->cabin_temp_k - th->coolant_temp_k)
                          / tau_coolant * dt;

    return 0;
}
'@

Set-Content -Path "models\habitat\src\habitat_thermal.c" -Value $thermalContent
Write-Host "  habitat_thermal.c replaced." -ForegroundColor Green

# ── Fix 3: Patch habitat_init.c - reduce degradation rate ────

Write-Host "[3/3] Tuning init parameters..." -ForegroundColor Yellow

$initPath = "models\habitat\src\habitat_init.c"
$initContent = Get-Content $initPath -Raw

# Reduce photothermal catalyst degradation (was too aggressive)
$initContent = $initContent -replace "catalyst_degradation  = 5\.0e-10", "catalyst_degradation  = 1.0e-11"

# Increase insulation R-value for regolith
$initContent = $initContent -replace "insulation_r_value  = 8\.0", "insulation_r_value  = 20.0"

Set-Content -Path $initPath -Value $initContent -NoNewline
Write-Host "  habitat_init.c patched." -ForegroundColor Green

Write-Host ""

# ── Commit and push ──────────────────────────────────────────

Write-Host "Pushing fixes to GitHub..." -ForegroundColor Yellow

git add -A
git commit -m "Fix physics: CO2 mass balance, O2 throttle, thermal model"
git push

Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  Physics fixes pushed! New build starting." -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  Watch with:  gh run watch" -ForegroundColor Yellow
Write-Host ""
