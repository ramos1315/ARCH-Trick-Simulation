/**
 * @file habitat.h
 * @brief ARCH – Autonomous Resilient Cislunar Habitat
 *
 * Top-level data structure for a six-crew lunar south-pole habitat.
 * Designed for NASA Trick simulation framework.
 *
 * Subsystems modeled:
 *   1. Cabin atmosphere  (CO₂ / O₂ mass balance)
 *   2. Life support       (OGS electrolysis, SBAR CO₂ scrubber, backup O₂)
 *   3. Power              (MSMR nuclear primary, fuel-cell backup, PMAD)
 *   4. Thermal            (active fluid loop + heaters, radiator rejection)
 *   5. Communications     (optical primary, RF backup link budget)
 *   6. Structure          (pressure shell stress, regolith shielding)
 *   7. Risk management    (threshold exceedance tracking)
 *
 * Reference: ARCH Conceptual System Analysis and Design Report, Feb 2026
 */

#ifndef HABITAT_H
#define HABITAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Physical / mission constants                                       */
/* ------------------------------------------------------------------ */
#define NUM_CREW            6
#define MISSION_DAYS        90       /* 3-month mission                 */
#define CABIN_VOLUME_M3     200.0    /* total pressurized volume  (m³)  */
#define CABIN_PRESSURE_KPA  101.325  /* 1 atm target                   */
#define CABIN_TEMP_NOM_K    295.15   /* 22 °C nominal                  */

/* Metabolic constants (per crew member) */
#define O2_CONSUMPTION_KG_S   0.000011574  /* ~1.0 kg/day per person   */
#define CO2_PRODUCTION_KG_S   0.000012731  /* ~1.1 kg/day per person   */
#define METABOLIC_HEAT_W      120.0        /* resting average per person*/

/* Safety thresholds */
#define CO2_WARNING_PPM       5000.0
#define CO2_DANGER_PPM        10000.0
#define O2_LOW_PCT            19.5
#define O2_HIGH_PCT           23.5
#define SOC_CRITICAL_PCT      10.0

/* ------------------------------------------------------------------ */
/*  Cabin Atmosphere State                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    double co2_ppm;            /* current CO₂ concentration (ppm)      */
    double o2_pct;             /* current O₂ percentage (%)            */
    double n2_pct;             /* nitrogen percentage (%)              */
    double co2_mass_kg;        /* total CO₂ mass in cabin (kg)        */
    double o2_mass_kg;         /* total O₂ mass in cabin (kg)         */
    double n2_mass_kg;         /* total N₂ mass in cabin (kg)         */
    double total_pressure_kpa; /* cabin total pressure (kPa)          */
    double temperature_k;      /* cabin air temperature (K)           */
    double humidity_pct;       /* relative humidity (%)               */
    double cabin_volume_m3;    /* pressurized volume (m³)             */
} CabinAtmosphere;

/* ------------------------------------------------------------------ */
/*  Life Support Subsystem                                             */
/* ------------------------------------------------------------------ */

/* Oxygen Generation System (ISS-heritage electrolysis) */
typedef struct {
    double max_production_kg_s; /* max O₂ production rate (kg/s)      */
    double current_output_kg_s; /* current production rate (kg/s)     */
    double power_draw_w;        /* electrical power consumed (W)      */
    double water_consumed_kg_s; /* H₂O electrolysis consumption       */
    int    is_active;           /* 1 = running, 0 = off               */
    int    has_fault;           /* 1 = faulted                        */
    double runtime_hours;       /* accumulated run time               */
} OxygenGenSystem;

/* Backup oxygen: compressed cylinders + oxygen candles */
typedef struct {
    double tank_mass_kg;        /* remaining O₂ in compressed tanks   */
    double tank_initial_kg;     /* initial O₂ mass at mission start   */
    double injection_rate_kg_s; /* current backup injection rate      */
    double reserve_threshold_kg;/* minimum reserve before alarm       */
    int    candles_remaining;   /* number of oxygen candles left      */
    double candle_rate_kg_s;    /* O₂ per candle when lit (kg/s)      */
    double candle_duration_s;   /* burn time per candle (s)           */
    int    candle_active;       /* 1 = candle currently burning       */
    double candle_burn_elapsed; /* time current candle has been lit   */
    double days_of_backup;      /* estimated remaining backup days    */
} BackupOxygen;

/* SBAR CO₂ scrubber (Sorbent-Based Atmosphere Revitalization) */
typedef struct {
    double max_removal_mol_s;   /* max CO₂ removal capacity (mol/s)  */
    double current_removal_mol_s;/* actual removal rate (mol/s)       */
    double sorbent_efficiency;  /* 0.0–1.0, degrades over time       */
    double degradation_rate;    /* efficiency loss per second         */
    double power_draw_w;        /* electrical power consumed (W)      */
    int    is_active;
    int    has_fault;
    int    bed_selector;        /* 0 or 1 – dual-bed swing cycle     */
    double cycle_timer_s;       /* time in current sorb/desorb cycle  */
    double cycle_period_s;      /* half-cycle duration (s)            */
} CO2Scrubber;

/* Photothermal CO₂ conversion reactor (advanced concept) */
typedef struct {
    double catalyst_area_m2;    /* active catalyst surface area       */
    double conversion_mol_s;    /* CO₂ converted to useful products   */
    double capture_efficiency;  /* fraction of cabin CO₂ captured     */
    double water_consumption_kg_s;
    double power_draw_w;
    double catalyst_degradation;/* fractional degradation per second  */
    int    is_active;
    int    has_fault;
    double trip_probability;    /* probability of random trip per step*/
    double co_leakage_ppm;      /* CO byproduct leakage              */
} PhotothermalReactor;

/* Aggregated life support */
typedef struct {
    OxygenGenSystem   ogs;
    BackupOxygen      backup_o2;
    CO2Scrubber       sbar;
    PhotothermalReactor phototherm;
    double total_power_w;       /* combined LS power draw             */
} LifeSupport;

/* ------------------------------------------------------------------ */
/*  Power Subsystem                                                    */
/* ------------------------------------------------------------------ */

/* Molten Salt and Metal Reactor (MSMR) – primary nuclear power */
typedef struct {
    double thermal_output_w;    /* reactor thermal power (W)          */
    double electrical_output_w; /* net electrical after conversion     */
    double conversion_efficiency;/* thermal-to-electric η             */
    double max_thermal_w;       /* design limit (500 kWth)            */
    int    is_active;
    int    scram;               /* 1 = emergency shutdown             */
    double runtime_hours;
    double fuel_remaining_pct;  /* fuel burnup tracking               */
} NuclearReactor;

/* Fuel cell backup */
typedef struct {
    double max_output_w;        /* peak electrical output (W)         */
    double current_output_w;    /* present output (W)                 */
    double h2_remaining_kg;     /* hydrogen fuel remaining            */
    double o2_remaining_kg;     /* oxidizer remaining                 */
    double h2_consumption_kg_s; /* H₂ usage rate                     */
    double efficiency;          /* electrical efficiency              */
    int    is_active;
    int    has_fault;
} FuelCell;

/* Battery energy storage */
typedef struct {
    double capacity_kwh;        /* total nameplate capacity           */
    double soc_pct;             /* state of charge (0–100 %)         */
    double charge_rate_w;       /* current charge rate (W)            */
    double discharge_rate_w;    /* current discharge rate (W)         */
    double max_charge_w;
    double max_discharge_w;
    double efficiency;          /* round-trip efficiency              */
    double temperature_k;       /* battery pack temperature           */
} BatteryBank;

/* Power Management and Distribution */
typedef struct {
    double total_generation_w;  /* sum of all sources                 */
    double total_load_w;        /* sum of all loads                   */
    double bus_voltage_v;       /* main DC bus voltage                */
    int    load_shed_active;    /* 1 = non-critical loads shed        */
    double survival_load_w;     /* minimum crew-survival load         */
    int    fault_isolated;      /* 1 = a branch has been isolated     */
} PMAD;

/* Aggregated power */
typedef struct {
    NuclearReactor  msmr;
    FuelCell        fuel_cell;
    BatteryBank     battery;
    PMAD            pmad;
    double baseline_demand_w;   /* ~53,880 W from report              */
} PowerSystem;

/* ------------------------------------------------------------------ */
/*  Thermal Control Subsystem                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    double cabin_temp_k;        /* interior air temperature (K)       */
    double coolant_temp_k;      /* fluid loop temperature (K)         */
    double radiator_rejection_w;/* heat rejected to space (W)         */
    double heater_output_w;     /* electric heater power (W)          */
    double metabolic_heat_w;    /* crew + equipment waste heat (W)    */
    double equipment_heat_w;    /* electronics dissipation (W)        */
    double radiator_area_m2;    /* exterior radiator area             */
    double insulation_r_value;  /* regolith + shell insulation        */
    double exterior_temp_k;     /* lunar surface temperature (K)      */
    double heater_power_draw_w; /* electrical power for heaters       */
    int    pump_active;         /* coolant pump status                */
} ThermalControl;

/* ------------------------------------------------------------------ */
/*  Communications Subsystem                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    /* Optical (laser) primary link */
    double optical_data_rate_mbps;
    double optical_power_w;
    int    optical_link_up;
    double optical_signal_margin_db;

    /* RF backup link */
    double rf_data_rate_kbps;
    double rf_power_w;
    int    rf_link_up;
    double rf_signal_margin_db;

    /* Relay satellite */
    int    relay_visible;       /* line-of-sight to relay sat         */
    double relay_elevation_deg; /* elevation angle                    */

    /* Aggregated */
    double total_power_w;
    int    comm_blackout;       /* 1 = no link available              */
} CommSystem;

/* ------------------------------------------------------------------ */
/*  Structural Subsystem                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    double shell_thickness_m;       /* Ti-Al alloy shell thickness    */
    double regolith_depth_m;        /* overburden depth (≥ 3.5 m)    */
    double internal_pressure_kpa;   /* current internal pressure      */
    double hoop_stress_mpa;         /* calculated hoop stress         */
    double allowable_stress_mpa;    /* material allowable             */
    double safety_factor;           /* allowable / actual             */
    double radiation_dose_msv_day;  /* crew radiation dose rate       */
    int    airlock_pressurized;     /* airlock state                  */
    int    hull_breach;             /* 1 = breach detected            */
} Structure;

/* ------------------------------------------------------------------ */
/*  Risk Management / Metrics                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    /* Cumulative time over threshold (seconds) */
    double co2_warning_time_s;
    double co2_danger_time_s;
    double o2_low_time_s;
    double soc_critical_time_s;
    double co_leakage_time_s;

    /* Crew safety score (0–100, higher = safer) */
    double crew_safety_score;
    int    risk_level;          /* 0=LOW, 1=MODERATE, 2=ELEVATED,
                                   3=HIGH, 4=CRITICAL               */

    /* Backup consumable margins */
    double o2_tank_margin_days;
    double candle_margin_days;
    double fuel_cell_margin_days;

    /* Event counters */
    int    ogs_fault_count;
    int    sbar_fault_count;
    int    reactor_scram_count;
    int    comm_blackout_count;
    int    phototherm_trip_count;
} RiskMetrics;

/* ------------------------------------------------------------------ */
/*  Top-Level Habitat Structure                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    /* Mission clock */
    double mission_elapsed_time_s;
    int    mission_day;

    /* Crew */
    int    crew_count;
    double crew_activity_factor; /* 1.0 = nominal metabolic rate     */

    /* Subsystems */
    CabinAtmosphere cabin;
    LifeSupport     life_support;
    PowerSystem     power;
    ThermalControl  thermal;
    CommSystem      comm;
    Structure       structure;
    RiskMetrics     risk;

    /* Simulation control */
    double dt;                   /* integration time step (s)        */
    int    sim_active;           /* master run flag                  */
} Habitat;

/* ------------------------------------------------------------------ */
/*  Function prototypes                                                */
/* ------------------------------------------------------------------ */

/* Initialization */
int habitat_init(Habitat *H);

/* Scheduled jobs (called each time step by Trick) */
int habitat_atmosphere_update(Habitat *H);
int habitat_life_support_update(Habitat *H);
int habitat_power_update(Habitat *H);
int habitat_thermal_update(Habitat *H);
int habitat_comm_update(Habitat *H);
int habitat_structure_update(Habitat *H);
int habitat_risk_update(Habitat *H);

/* Shutdown / cleanup */
int habitat_shutdown(Habitat *H);

#ifdef __cplusplus
}
#endif

#endif /* HABITAT_H */
