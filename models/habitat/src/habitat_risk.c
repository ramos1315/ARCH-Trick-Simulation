/**
 * @file habitat_risk.c
 * @brief Risk management metrics and crew safety scoring.
 *
 * Continuously checks safety thresholds and accumulates
 * time-over-limit metrics. Computes crew safety score
 * and qualitative risk level per the ARCH risk framework.
 */

#include "habitat/include/habitat.h"
#include <math.h>
#include <stdio.h>

int habitat_risk_update(Habitat *H)
{
    double dt = H->dt;
    RiskMetrics *rm = &H->risk;

    /* ---- Update mission clock ----------------------------------- */
    H->mission_elapsed_time_s += dt;
    H->mission_day = (int)(H->mission_elapsed_time_s / 86400.0);

    /* Check end of mission */
    if (H->mission_day >= MISSION_DAYS) {
        H->sim_active = 0;
    }

    /* ---- Threshold exceedance tracking -------------------------- */
    if (H->cabin.co2_ppm > CO2_WARNING_PPM)
        rm->co2_warning_time_s += dt;
    if (H->cabin.co2_ppm > CO2_DANGER_PPM)
        rm->co2_danger_time_s += dt;
    if (H->cabin.o2_pct < O2_LOW_PCT)
        rm->o2_low_time_s += dt;
    if (H->power.battery.soc_pct < SOC_CRITICAL_PCT)
        rm->soc_critical_time_s += dt;
    if (H->life_support.phototherm.co_leakage_ppm > 25.0)
        rm->co_leakage_time_s += dt;

    /* ---- Fault counters ----------------------------------------- */
    /* (incremented elsewhere when faults occur) */

    /* ---- Backup consumable margins ------------------------------ */
    double daily_o2 = O2_CONSUMPTION_KG_S * H->crew_count * 86400.0;
    rm->o2_tank_margin_days = (daily_o2 > 0.0)
        ? H->life_support.backup_o2.tank_mass_kg / daily_o2
        : 0.0;
    rm->candle_margin_days = (daily_o2 > 0.0)
        ? (H->life_support.backup_o2.candles_remaining
           * H->life_support.backup_o2.candle_rate_kg_s
           * H->life_support.backup_o2.candle_duration_s) / daily_o2
        : 0.0;

    double fc_energy_j = H->power.fuel_cell.h2_remaining_kg
                         * 141.8e6 * H->power.fuel_cell.efficiency;
    rm->fuel_cell_margin_days = fc_energy_j
                                / (H->power.baseline_demand_w * 86400.0);

    /* ---- Crew Safety Score (0–100) ------------------------------ */
    double mission_s = H->mission_elapsed_time_s;
    if (mission_s < 1.0) mission_s = 1.0;

    /* Penalty: fraction of mission time in violation */
    double penalty = 0.0;
    penalty += (rm->co2_warning_time_s / mission_s) * 15.0;
    penalty += (rm->co2_danger_time_s  / mission_s) * 30.0;
    penalty += (rm->o2_low_time_s      / mission_s) * 30.0;
    penalty += (rm->soc_critical_time_s / mission_s) * 15.0;
    penalty += (rm->co_leakage_time_s  / mission_s) * 10.0;

    rm->crew_safety_score = 100.0 - penalty;
    if (rm->crew_safety_score < 0.0)
        rm->crew_safety_score = 0.0;

    /* ---- Qualitative risk level --------------------------------- */
    if (rm->crew_safety_score >= 90.0)
        rm->risk_level = 0;  /* LOW */
    else if (rm->crew_safety_score >= 75.0)
        rm->risk_level = 1;  /* MODERATE */
    else if (rm->crew_safety_score >= 50.0)
        rm->risk_level = 2;  /* ELEVATED */
    else if (rm->crew_safety_score >= 25.0)
        rm->risk_level = 3;  /* HIGH */
    else
        rm->risk_level = 4;  /* CRITICAL */

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Shutdown – print final summary                                     */
/* ------------------------------------------------------------------ */
int habitat_shutdown(Habitat *H)
{
    static const char *risk_names[] = {
        "LOW", "MODERATE", "ELEVATED", "HIGH", "CRITICAL"
    };

    printf("\n");
    printf("============================================================\n");
    printf("  ARCH Habitat Simulation – End of Mission Summary\n");
    printf("============================================================\n");
    printf("  Mission duration:        %d days\n", H->mission_day);
    printf("  Final CO₂:              %.1f ppm\n", H->cabin.co2_ppm);
    printf("  Final O₂:               %.2f %%\n", H->cabin.o2_pct);
    printf("  Final cabin temp:        %.1f K (%.1f °C)\n",
           H->cabin.temperature_k,
           H->cabin.temperature_k - 273.15);
    printf("  Battery SOC:             %.1f %%\n", H->power.battery.soc_pct);
    printf("  Reactor runtime:         %.1f hours\n",
           H->power.msmr.runtime_hours);
    printf("  Backup O₂ remaining:    %.1f kg (%.1f days)\n",
           H->life_support.backup_o2.tank_mass_kg,
           H->risk.o2_tank_margin_days);
    printf("  O₂ candles remaining:   %d\n",
           H->life_support.backup_o2.candles_remaining);
    printf("  Crew safety score:       %.1f / 100\n",
           H->risk.crew_safety_score);
    printf("  Risk level:              %s\n",
           risk_names[H->risk.risk_level]);
    printf("------------------------------------------------------------\n");
    printf("  CO₂ warning time:       %.1f hours\n",
           H->risk.co2_warning_time_s / 3600.0);
    printf("  CO₂ danger time:        %.1f hours\n",
           H->risk.co2_danger_time_s / 3600.0);
    printf("  O₂ low time:            %.1f hours\n",
           H->risk.o2_low_time_s / 3600.0);
    printf("  SOC critical time:       %.1f hours\n",
           H->risk.soc_critical_time_s / 3600.0);
    printf("  Comm blackout events:    %d\n",
           H->risk.comm_blackout_count);
    printf("  Photothermal trips:      %d\n",
           H->risk.phototherm_trip_count);
    printf("  Radiation dose (total):  %.1f mSv\n",
           H->structure.radiation_dose_msv_day * H->mission_day);
    printf("  Structural SF:           %.1f\n",
           H->structure.safety_factor);
    printf("============================================================\n\n");

    return 0;
}
