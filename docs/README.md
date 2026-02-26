# ARCH – Autonomous Resilient Cislunar Habitat
## NASA Trick Simulation

### Overview

This is a NASA Trick simulation of the **ARCH (Autonomous Resilient Cislunar Habitat)**, a modular lunar habitat designed for a six-crew, three-month mission at the Moon's South Pole. The simulation models all major subsystems identified in the Conceptual System Analysis and Design Report (February 2026) for NASA's Simulation Exploration Experience 2026.

### Subsystems Modeled

| Subsystem | Key Models | Parameters from Report |
|-----------|-----------|----------------------|
| **Cabin Atmosphere** | CO₂/O₂ mass balance, ideal gas law | 200 m³ volume, 1 atm, 6 crew |
| **Life Support** | OGS electrolysis, SBAR CO₂ scrubber, photothermal reactor, backup O₂ | 8,000 W power draw, 0.001578 mol/s CO₂ removal |
| **Power** | MSMR nuclear (500 kWth), fuel cell backup, battery, PMAD | 53.88 kW baseline demand |
| **Thermal** | Active fluid loop, radiators, heaters | ISS-heritage ATCS concept |
| **Communications** | Optical laser primary, RF backup, relay satellite | 355 W total, 1 Gbps optical |
| **Structure** | Pressure shell stress, regolith shielding, radiation dose | Ti-Al alloy, 3.5 m regolith |
| **Risk Management** | Threshold exceedance, crew safety score, risk level | CO₂/O₂/SOC limits per report |

### Directory Structure

```
ARCH_Trick_Sim/
├── S_define                          # Trick simulation definition
├── S_overrides.mk                    # Build configuration
├── models/
│   └── habitat/
│       ├── include/
│       │   └── habitat.h             # All data structures & constants
│       └── src/
│           ├── habitat_init.c        # Initialization (BOL parameters)
│           ├── habitat_atmosphere.c  # Cabin air + life support dynamics
│           ├── habitat_power.c       # Nuclear + fuel cell + battery + PMAD
│           ├── habitat_thermal.c     # Thermal control (lumped parameter)
│           ├── habitat_comm.c        # Optical/RF comm + relay visibility
│           ├── habitat_structure.c   # Structural integrity + radiation
│           └── habitat_risk.c        # Risk metrics + safety scoring
├── RUN_nominal/
│   └── input.py                      # 90-day nominal mission
├── RUN_failure/
│   └── input.py                      # OGS failure at day 30
└── docs/
    └── README.md                     # This file
```

### Prerequisites

- **NASA Trick** (version 19.x or later): https://github.com/nasa/trick
- **GCC** or compatible C/C++ compiler
- **Python** 3.x (for Trick input files)
- **Make** (GNU Make)

### Building

```bash
# Set TRICK_HOME environment variable
export TRICK_HOME=/path/to/trick

# From the ARCH_Trick_Sim directory:
trick-CP
```

This will process the `S_define`, compile all model source files, and produce the executable `S_main_*.exe`.

### Running

```bash
# Nominal 90-day mission
./S_main_*.exe RUN_nominal/input.py

# OGS failure scenario
./S_main_*.exe RUN_failure/input.py
```

### Data Output

Simulation data is recorded to ASCII files in `RUN_*/log_ARCH_*/` at 60-second intervals. Key variables recorded include:

- **Cabin**: CO₂ ppm, O₂ %, temperature, pressure
- **Power**: reactor output, battery SOC, load shedding status
- **Life Support**: OGS output, SBAR efficiency, backup O₂ remaining
- **Risk**: crew safety score, cumulative threshold violation times

Use Trick's **trick-dp** (data products) or export to CSV for plotting.

### Key Design Decisions

1. **1-second time step**: Captures transient dynamics in atmosphere and power systems while keeping computational cost manageable for a 90-day mission (~7.8M steps).

2. **Mass-balance atmosphere model**: CO₂ and O₂ tracked as mass variables, converted to ppm/% via ideal gas law. This matches the approach described in the report's code verification model.

3. **Photothermal reactor with stochastic trips**: Random fault injection via probability-per-step models realistic operational unavailability, consistent with the Monte Carlo risk framework in the report.

4. **Supervisory backup O₂ logic**: Compressed tank injection activates when O₂ drops below 19.5%, with oxygen candles as a tertiary backup — matching the three-tier redundancy described in the report.

5. **PMAD load shedding**: Automatic non-critical load shedding when generation + battery cannot meet demand, ensuring crew survival loads are always prioritized.

### Extending the Simulation

- **Monte Carlo**: Wrap runs in a Python script that perturbs `crew_activity_factor`, `trip_probability`, `sorbent_efficiency`, etc., and aggregate results — replicating the report's probabilistic risk assessment.
- **Additional failure modes**: Add Trick events for SBAR failure, reactor SCRAM, hull breach, comm relay loss, etc.
- **Higher fidelity thermal**: Replace lumped-parameter model with multi-node finite difference for regolith/shell/cabin coupling.
- **EVA modeling**: Add airlock cycling, suit consumables, and crew scheduling.

### Traceability to Report

All numerical parameters (power budgets, removal rates, volumes, masses) are traced to Table 1 and the subsystem descriptions in the ARCH Conceptual System Analysis and Design Report. Comments in `habitat.h` and `habitat_init.c` note the source of each value.

### Authors

ARCH Capstone Team — Embry-Riddle Aeronautical University, ENGR 490
