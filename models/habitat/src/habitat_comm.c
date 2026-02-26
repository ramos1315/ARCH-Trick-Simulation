/**
 * @file habitat_comm.c
 * @brief Communications subsystem for the ARCH lunar habitat.
 *
 * Models relay satellite visibility (simplified orbital model),
 * optical and RF link availability, and blackout detection.
 */

#include "habitat/include/habitat.h"
#include <math.h>

/* Relay satellite orbital parameters (simplified) */
#define RELAY_ORBITAL_PERIOD_S  43200.0  /* ~12-hour lunar orbit */
#define VISIBILITY_FRACTION     0.65     /* fraction of orbit with LOS */

int habitat_comm_update(Habitat *H)
{
    CommSystem *cm = &H->comm;
    double met = H->mission_elapsed_time_s;

    /* ---- Relay satellite visibility (sinusoidal approximation) --- */
    double phase = fmod(met, RELAY_ORBITAL_PERIOD_S)
                   / RELAY_ORBITAL_PERIOD_S;
    double elevation = 90.0 * sin(2.0 * M_PI * phase);

    cm->relay_elevation_deg = elevation;
    cm->relay_visible = (elevation > 5.0) ? 1 : 0;  /* 5° mask angle */

    /* ---- Optical link ------------------------------------------- */
    if (cm->relay_visible && cm->optical_signal_margin_db > 0.0) {
        cm->optical_link_up = 1;
    } else {
        cm->optical_link_up = 0;
    }

    /* ---- RF backup link ----------------------------------------- */
    if (cm->relay_visible && cm->rf_signal_margin_db > 0.0) {
        cm->rf_link_up = 1;
    } else {
        cm->rf_link_up = 0;
    }

    /* ---- Blackout detection ------------------------------------- */
    if (!cm->optical_link_up && !cm->rf_link_up) {
        if (!cm->comm_blackout) {
            cm->comm_blackout = 1;
            H->risk.comm_blackout_count++;
        }
    } else {
        cm->comm_blackout = 0;
    }

    /* Power draw (only active links consume full power) */
    cm->total_power_w = 0.0;
    if (cm->optical_link_up)
        cm->total_power_w += cm->optical_power_w;
    if (cm->rf_link_up)
        cm->total_power_w += cm->rf_power_w;
    if (cm->total_power_w < 50.0)
        cm->total_power_w = 50.0;  /* standby power */

    return 0;
}
