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
