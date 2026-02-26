/**
 * @file habitat_init.c
 * @brief Initialization of the ARCH habitat for Trick simulation.
 *
 * Sets all subsystem states to beginning-of-mission values derived
 * from the Conceptual System Analysis and Design Report.
 */

#include "habitat/include/habitat.h"
#include <string.h>
#include <math.h>

/* Molar masses (kg/mol) */
#define MM_O2   0.032
#define MM_CO2  0.044
#define MM_N2   0.028
#define MM_AIR  0.029   /* approximate for cabin mix */
#define R_GAS   8.314   /* J/(mol·K) */

int habitat_init(Habitat *H)
{
    memset(H, 0, sizeof(Habitat));

    /* -------------------------------------------------------------- */
    /*  Mission & crew                                                 */
    /* -------------------------------------------------------------- */
    H->mission_elapsed_time_s = 0.0;
    H->mission_day            = 0;
    H->crew_count             = NUM_CREW;
    H->crew_activity_factor   = 1.0;
    H->dt                     = 1.0;   /* 1-second time step */
    H->sim_active             = 1;

    /* -------------------------------------------------------------- */
    /*  Cabin atmosphere – ISS-like at 1 atm, 21% O₂                  */
    /* -------------------------------------------------------------- */
    H->cabin.cabin_volume_m3   = CABIN_VOLUME_M3;
    H->cabin.temperature_k     = CABIN_TEMP_NOM_K;
    H->cabin.total_pressure_kpa = CABIN_PRESSURE_KPA;
    H->cabin.humidity_pct      = 45.0;

    /* Compute initial gas masses from ideal gas law:
       n = PV / RT  →  m = n * MM                                    */
    double n_total = (CABIN_PRESSURE_KPA * 1000.0 * CABIN_VOLUME_M3)
                     / (R_GAS * CABIN_TEMP_NOM_K);       /* total moles */

    H->cabin.o2_pct   = 20.9;
    H->cabin.n2_pct   = 78.1;
    H->cabin.co2_ppm  = 400.0;   /* start at ambient-equivalent */

    double n_o2  = n_total * (H->cabin.o2_pct / 100.0);
    double n_n2  = n_total * (H->cabin.n2_pct / 100.0);
    double n_co2 = n_total * (H->cabin.co2_ppm / 1.0e6);

    H->cabin.o2_mass_kg  = n_o2  * MM_O2;
    H->cabin.n2_mass_kg  = n_n2  * MM_N2;
    H->cabin.co2_mass_kg = n_co2 * MM_CO2;

    /* -------------------------------------------------------------- */
    /*  Life Support                                                   */
    /* -------------------------------------------------------------- */

    /* OGS – ISS-heritage electrolysis */
    H->life_support.ogs.max_production_kg_s = NUM_CREW * O2_CONSUMPTION_KG_S * 1.2;
    H->life_support.ogs.current_output_kg_s = H->life_support.ogs.max_production_kg_s;
    H->life_support.ogs.power_draw_w        = 2500.0;  /* estimated */
    H->life_support.ogs.water_consumed_kg_s = H->life_support.ogs.max_production_kg_s
                                              * (18.015 / 32.0) * 2.0; /* 2H₂O → O₂ */
    H->life_support.ogs.is_active           = 1;
    H->life_support.ogs.has_fault           = 0;
    H->life_support.ogs.runtime_hours       = 0.0;

    /* Backup O₂ – compressed tanks + candles (11 days backup per report) */
    double daily_o2_kg = NUM_CREW * O2_CONSUMPTION_KG_S * 86400.0;
    H->life_support.backup_o2.tank_initial_kg    = daily_o2_kg * 11.0;
    H->life_support.backup_o2.tank_mass_kg       = H->life_support.backup_o2.tank_initial_kg;
    H->life_support.backup_o2.injection_rate_kg_s = 0.0;
    H->life_support.backup_o2.reserve_threshold_kg = daily_o2_kg * 2.0;
    H->life_support.backup_o2.candles_remaining   = 30;
    H->life_support.backup_o2.candle_rate_kg_s    = 0.000382; /* ~33 kg O₂/day equiv */
    H->life_support.backup_o2.candle_duration_s   = 600.0;    /* 10 min per candle */
    H->life_support.backup_o2.candle_active        = 0;
    H->life_support.backup_o2.candle_burn_elapsed  = 0.0;
    H->life_support.backup_o2.days_of_backup       = 11.0;

    /* SBAR CO₂ scrubber */
    H->life_support.sbar.max_removal_mol_s    = 0.002083;  /* from report */
    H->life_support.sbar.current_removal_mol_s = 0.002083;
    H->life_support.sbar.sorbent_efficiency   = 1.0;
    H->life_support.sbar.degradation_rate     = 1.0e-10;   /* very slow */
    H->life_support.sbar.power_draw_w         = 1500.0;
    H->life_support.sbar.is_active            = 1;
    H->life_support.sbar.has_fault            = 0;
    H->life_support.sbar.bed_selector         = 0;
    H->life_support.sbar.cycle_timer_s        = 0.0;
    H->life_support.sbar.cycle_period_s       = 900.0;     /* 15-min half-cycle */

    /* Photothermal CO₂ reactor */
    H->life_support.phototherm.catalyst_area_m2     = 10.0;
    H->life_support.phototherm.conversion_mol_s     = 0.001258; /* from report */
    H->life_support.phototherm.capture_efficiency    = 0.85;
    H->life_support.phototherm.water_consumption_kg_s = 0.0001;
    H->life_support.phototherm.power_draw_w          = 2000.0;
    H->life_support.phototherm.catalyst_degradation  = 1.0e-11;
    H->life_support.phototherm.is_active             = 1;
    H->life_support.phototherm.has_fault             = 0;
    H->life_support.phototherm.trip_probability      = 1.0e-6;
    H->life_support.phototherm.co_leakage_ppm        = 0.0;

    H->life_support.total_power_w = 8000.0;  /* from Table 1 */

    /* -------------------------------------------------------------- */
    /*  Power System                                                   */
    /* -------------------------------------------------------------- */

    /* MSMR Nuclear Reactor – 500 kWth */
    H->power.msmr.max_thermal_w         = 500000.0;
    H->power.msmr.thermal_output_w      = 500000.0;
    H->power.msmr.conversion_efficiency = 0.30;  /* ~30% Brayton/Stirling */
    H->power.msmr.electrical_output_w   = 500000.0 * 0.30; /* 150 kWe */
    H->power.msmr.is_active             = 1;
    H->power.msmr.scram                 = 0;
    H->power.msmr.runtime_hours         = 0.0;
    H->power.msmr.fuel_remaining_pct    = 100.0;

    /* Fuel cell backup */
    H->power.fuel_cell.max_output_w       = 20000.0;   /* 20 kW peak */
    H->power.fuel_cell.current_output_w   = 0.0;
    H->power.fuel_cell.h2_remaining_kg    = 200.0;
    H->power.fuel_cell.o2_remaining_kg    = 1600.0;
    H->power.fuel_cell.h2_consumption_kg_s = 0.0;
    H->power.fuel_cell.efficiency         = 0.60;
    H->power.fuel_cell.is_active          = 0;  /* standby */
    H->power.fuel_cell.has_fault          = 0;

    /* Battery bank */
    H->power.battery.capacity_kwh    = 100.0;
    H->power.battery.soc_pct         = 95.0;
    H->power.battery.charge_rate_w   = 0.0;
    H->power.battery.discharge_rate_w = 0.0;
    H->power.battery.max_charge_w    = 25000.0;
    H->power.battery.max_discharge_w = 50000.0;
    H->power.battery.efficiency      = 0.92;
    H->power.battery.temperature_k   = 293.15;

    /* PMAD */
    H->power.pmad.total_generation_w = H->power.msmr.electrical_output_w;
    H->power.pmad.total_load_w       = 53880.0;  /* baseline from report */
    H->power.pmad.bus_voltage_v      = 120.0;
    H->power.pmad.load_shed_active   = 0;
    H->power.pmad.survival_load_w    = 15000.0;
    H->power.pmad.fault_isolated     = 0;

    H->power.baseline_demand_w = 53880.0;

    /* -------------------------------------------------------------- */
    /*  Thermal Control                                                */
    /* -------------------------------------------------------------- */
    H->thermal.cabin_temp_k        = CABIN_TEMP_NOM_K;
    H->thermal.coolant_temp_k      = 280.0;
    H->thermal.radiator_rejection_w = 0.0;
    H->thermal.heater_output_w     = 0.0;
    H->thermal.metabolic_heat_w    = NUM_CREW * METABOLIC_HEAT_W;
    H->thermal.equipment_heat_w    = 5000.0;
    H->thermal.radiator_area_m2    = 50.0;
    H->thermal.insulation_r_value  = 20.0;  /* regolith + shell */
    H->thermal.exterior_temp_k     = 100.0; /* ~south pole shadow */
    H->thermal.heater_power_draw_w = 0.0;
    H->thermal.pump_active         = 1;

    /* -------------------------------------------------------------- */
    /*  Communications                                                 */
    /* -------------------------------------------------------------- */
    H->comm.optical_data_rate_mbps   = 1000.0;  /* 1 Gbps laser */
    H->comm.optical_power_w          = 250.0;
    H->comm.optical_link_up          = 1;
    H->comm.optical_signal_margin_db = 6.0;

    H->comm.rf_data_rate_kbps      = 2000.0;
    H->comm.rf_power_w             = 105.0;
    H->comm.rf_link_up             = 1;
    H->comm.rf_signal_margin_db    = 3.0;

    H->comm.relay_visible          = 1;
    H->comm.relay_elevation_deg    = 45.0;
    H->comm.total_power_w          = 355.0;  /* from Table 1 */
    H->comm.comm_blackout          = 0;

    /* -------------------------------------------------------------- */
    /*  Structure                                                      */
    /* -------------------------------------------------------------- */
    H->structure.shell_thickness_m     = 0.012;     /* 12 mm Ti-Al */
    H->structure.regolith_depth_m      = 3.5;       /* from report */
    H->structure.internal_pressure_kpa = CABIN_PRESSURE_KPA;
    H->structure.allowable_stress_mpa  = 880.0;     /* Ti-6Al-4V   */
    H->structure.safety_factor         = 2.0;
    H->structure.radiation_dose_msv_day = 0.3;      /* shielded    */
    H->structure.airlock_pressurized   = 1;
    H->structure.hull_breach           = 0;

    /* Hoop stress: σ = P·r / t  (thin-walled sphere approx) */
    double dome_radius_m = 4.0;  /* hemispherical module radius */
    H->structure.hoop_stress_mpa =
        (CABIN_PRESSURE_KPA * 1e-3) * dome_radius_m
        / (2.0 * H->structure.shell_thickness_m);

    /* -------------------------------------------------------------- */
    /*  Risk metrics – all zeroed                                      */
    /* -------------------------------------------------------------- */
    /* Already zeroed by memset */

    return 0;
}
