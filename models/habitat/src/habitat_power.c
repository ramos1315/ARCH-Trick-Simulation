/**
 * @file habitat_power.c
 * @brief Power subsystem dynamics for the ARCH lunar habitat.
 *
 * Models:
 *   - MSMR nuclear reactor (500 kWth, ~150 kWe)
 *   - Fuel cell backup (20 kW peak)
 *   - Battery bank (100 kWh, charge/discharge)
 *   - PMAD load balancing, fault isolation, load shedding
 */

#include "../include/habitat.h"
#include <math.h>

int habitat_power_update(Habitat *H)
{
    double dt = H->dt;
    PowerSystem *pwr = &H->power;

    /* ============================================================== */
    /*  1. Nuclear reactor output                                      */
    /* ============================================================== */
    NuclearReactor *rx = &pwr->msmr;

    if (rx->is_active && !rx->scram) {
        /* Slow fuel burnup over mission (~negligible for 90 days) */
        rx->fuel_remaining_pct -= (dt / (86400.0 * 365.0 * 10.0)) * 100.0;
        if (rx->fuel_remaining_pct < 0.0)
            rx->fuel_remaining_pct = 0.0;

        rx->thermal_output_w    = rx->max_thermal_w
                                  * (rx->fuel_remaining_pct / 100.0);
        rx->electrical_output_w = rx->thermal_output_w
                                  * rx->conversion_efficiency;
        rx->runtime_hours += dt / 3600.0;
    } else {
        rx->electrical_output_w = 0.0;
        rx->thermal_output_w    = 0.0;
    }

    /* ============================================================== */
    /*  2. Compute total load                                          */
    /* ============================================================== */
    double total_load = H->life_support.total_power_w
                      + H->thermal.heater_power_draw_w
                      + H->comm.total_power_w
                      + 5000.0;  /* avionics, lighting, misc */

    pwr->pmad.total_load_w = total_load;

    /* ============================================================== */
    /*  3. PMAD – generation vs. load balancing                        */
    /* ============================================================== */
    double generation = rx->electrical_output_w;

    /* Fuel cell activation logic: engage if reactor output < load */
    FuelCell *fc = &pwr->fuel_cell;
    if (generation < total_load && !fc->has_fault && fc->h2_remaining_kg > 0.0) {
        fc->is_active = 1;
        double deficit = total_load - generation;
        if (deficit > fc->max_output_w)
            deficit = fc->max_output_w;
        fc->current_output_w = deficit;

        /* H₂ consumption: P = m_dot * HHV * η → m_dot = P/(HHV*η) */
        double HHV_H2 = 141.8e6;  /* J/kg */
        fc->h2_consumption_kg_s = fc->current_output_w
                                  / (HHV_H2 * fc->efficiency);
        fc->h2_remaining_kg -= fc->h2_consumption_kg_s * dt;
        fc->o2_remaining_kg -= fc->h2_consumption_kg_s * 8.0 * dt;

        if (fc->h2_remaining_kg <= 0.0) {
            fc->h2_remaining_kg  = 0.0;
            fc->current_output_w = 0.0;
            fc->is_active        = 0;
        }

        generation += fc->current_output_w;
    } else if (generation >= total_load) {
        fc->is_active        = 0;
        fc->current_output_w = 0.0;
        fc->h2_consumption_kg_s = 0.0;
    }

    pwr->pmad.total_generation_w = generation;

    /* ============================================================== */
    /*  4. Battery charge / discharge                                  */
    /* ============================================================== */
    BatteryBank *bat = &pwr->battery;
    double surplus = generation - total_load;

    if (surplus > 0.0) {
        /* Charge battery */
        double charge = surplus;
        if (charge > bat->max_charge_w)
            charge = bat->max_charge_w;

        double energy_kwh = (charge * bat->efficiency * dt) / 3.6e6;
        bat->soc_pct += (energy_kwh / bat->capacity_kwh) * 100.0;
        if (bat->soc_pct > 100.0)
            bat->soc_pct = 100.0;

        bat->charge_rate_w    = charge;
        bat->discharge_rate_w = 0.0;
    } else if (surplus < 0.0) {
        /* Discharge battery to cover deficit */
        double deficit = -surplus;
        if (deficit > bat->max_discharge_w)
            deficit = bat->max_discharge_w;

        double energy_kwh = (deficit * dt) / (3.6e6 * bat->efficiency);
        bat->soc_pct -= (energy_kwh / bat->capacity_kwh) * 100.0;
        if (bat->soc_pct < 0.0)
            bat->soc_pct = 0.0;

        bat->discharge_rate_w = deficit;
        bat->charge_rate_w    = 0.0;
    } else {
        bat->charge_rate_w    = 0.0;
        bat->discharge_rate_w = 0.0;
    }

    /* ============================================================== */
    /*  5. Load shedding                                               */
    /* ============================================================== */
    double available = generation
                       + (bat->soc_pct > 5.0 ? bat->max_discharge_w : 0.0);

    if (available < total_load) {
        pwr->pmad.load_shed_active = 1;
        /* Shed non-critical loads: comms optical, science, lighting dim */
        /* Keep survival loads: LS, heaters, RF comm, avionics core */
    } else {
        pwr->pmad.load_shed_active = 0;
    }

    return 0;
}
