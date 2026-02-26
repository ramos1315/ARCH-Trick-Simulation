/**
 * @file habitat_thermal.c
 * @brief Thermal control subsystem for the ARCH lunar habitat.
 *
 * Simplified lumped-parameter thermal model:
 *   Q_in  = metabolic + equipment + heaters
 *   Q_out = radiator rejection + conduction through regolith/shell
 *   dT/dt = (Q_in - Q_out) / (m_air * Cp_air)
 */

#include "../include/habitat.h"
#include <math.h>

/* Air thermal properties */
#define CP_AIR      1005.0   /* J/(kg·K)  */
#define RHO_AIR     1.225    /* kg/m³ at ~1 atm, 20°C */

/* Stefan-Boltzmann for radiator */
#define SIGMA       5.67e-8
#define EMISSIVITY  0.85

int habitat_thermal_update(Habitat *H)
{
    double dt = H->dt;
    ThermalControl *th = &H->thermal;

    /* Total heat input */
    th->metabolic_heat_w = H->crew_count * H->crew_activity_factor
                           * METABOLIC_HEAT_W;
    double q_in = th->metabolic_heat_w + th->equipment_heat_w
                + th->heater_output_w;

    /* Radiator rejection (simplified Stefan-Boltzmann) */
    if (th->pump_active) {
        double T_rad = th->coolant_temp_k;
        th->radiator_rejection_w = EMISSIVITY * SIGMA * th->radiator_area_m2
                                   * (pow(T_rad, 4.0)
                                      - pow(th->exterior_temp_k, 4.0));
        if (th->radiator_rejection_w < 0.0)
            th->radiator_rejection_w = 0.0;
    } else {
        th->radiator_rejection_w = 0.0;
    }

    /* Conduction loss through regolith/shell */
    double R_total = th->insulation_r_value; /* K·m²/W simplified */
    double A_shell = 4.0 * M_PI * 4.0 * 4.0; /* surface area of dome */
    double q_cond  = A_shell * (th->cabin_temp_k - th->exterior_temp_k)
                     / R_total;

    double q_out = th->radiator_rejection_w + q_cond;

    /* Thermal mass of cabin air */
    double m_air = RHO_AIR * H->cabin.cabin_volume_m3;
    double dT = (q_in - q_out) / (m_air * CP_AIR) * dt;

    th->cabin_temp_k += dT;
    H->cabin.temperature_k = th->cabin_temp_k;

    /* Heater control: maintain ± 2 K of nominal */
    if (th->cabin_temp_k < (CABIN_TEMP_NOM_K - 2.0)) {
        th->heater_output_w     = 5000.0;  /* kick in heaters */
        th->heater_power_draw_w = 5000.0;
    } else if (th->cabin_temp_k > (CABIN_TEMP_NOM_K + 2.0)) {
        th->heater_output_w     = 0.0;
        th->heater_power_draw_w = 0.0;
        /* Could increase radiator flow – simplified here */
    } else {
        th->heater_output_w     = 0.0;
        th->heater_power_draw_w = 0.0;
    }

    /* Update coolant temperature (simplified: tracks cabin with lag) */
    double tau_coolant = 300.0; /* 5-minute thermal time constant */
    th->coolant_temp_k += (th->cabin_temp_k - th->coolant_temp_k)
                          / tau_coolant * dt;

    return 0;
}
