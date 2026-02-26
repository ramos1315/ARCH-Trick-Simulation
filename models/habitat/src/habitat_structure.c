/**
 * @file habitat_structure.c
 * @brief Structural integrity monitoring for the ARCH habitat.
 *
 * Tracks hoop stress vs. allowable, radiation dose accumulation,
 * and pressure integrity.
 */

#include "habitat/include/habitat.h"
#include <math.h>

int habitat_structure_update(Habitat *H)
{
    Structure *st = &H->structure;

    /* Update internal pressure from cabin atmosphere state */
    st->internal_pressure_kpa = H->cabin.total_pressure_kpa;

    /* Hoop stress recalculation: σ = P·r / (2·t) for sphere */
    double dome_radius_m = 4.0;
    st->hoop_stress_mpa = (st->internal_pressure_kpa * 1e-3)
                          * dome_radius_m
                          / (2.0 * st->shell_thickness_m);

    /* Safety factor */
    st->safety_factor = (st->hoop_stress_mpa > 0.0)
                        ? st->allowable_stress_mpa / st->hoop_stress_mpa
                        : 999.0;

    /* Radiation dose: regolith shielding attenuates GCR/SPE
       Rough model: dose decreases exponentially with depth */
    double unshielded_dose = 1.5;  /* mSv/day on lunar surface */
    double atten = exp(-st->regolith_depth_m / 1.2);  /* ~1.2 m half-value */
    st->radiation_dose_msv_day = unshielded_dose * atten;

    return 0;
}
