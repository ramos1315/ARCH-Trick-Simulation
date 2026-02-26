# RUN_nominal/input.py - ARCH Habitat Nominal 90-Day Mission
# Trick input file (Python syntax) for a full three-month mission
# with all subsystems operating nominally.

# Mission duration: 90 days in seconds
mission_duration = 90 * 24 * 3600  # 7,776,000 seconds

trick.exec_set_terminate_time(mission_duration)

# Integration time step: 1 second
trick.exec_set_freeze_frame(0.0)

# Real-time: OFF (run as fast as possible)
trick.real_time_disable()

# Crew
arch.habitat.crew_count          = 6
arch.habitat.crew_activity_factor = 1.0

# Cabin atmosphere initial conditions
arch.habitat.cabin.o2_pct   = 20.9
arch.habitat.cabin.co2_ppm  = 400.0
arch.habitat.cabin.temperature_k = 295.15

# Power - reactor at full output
arch.habitat.power.msmr.is_active = 1
arch.habitat.power.msmr.scram     = 0

# Life support - all systems active
arch.habitat.life_support.ogs.is_active         = 1
arch.habitat.life_support.sbar.is_active        = 1
arch.habitat.life_support.phototherm.is_active  = 1

# Thermal - pump active
arch.habitat.thermal.pump_active = 1

# Communications - both links up
arch.habitat.comm.optical_link_up = 1
arch.habitat.comm.rf_link_up      = 1

# Structure - no damage
arch.habitat.structure.hull_breach = 0

# Data recording
drg = trick.DRAscii("ARCH_Nominal")
drg.set_cycle(60.0)
drg.set_freq(trick.DR_Always)

# Mission clock
drg.add_variable("arch.habitat.mission_elapsed_time_s")
drg.add_variable("arch.habitat.mission_day")

# Cabin atmosphere
drg.add_variable("arch.habitat.cabin.co2_ppm")
drg.add_variable("arch.habitat.cabin.o2_pct")
drg.add_variable("arch.habitat.cabin.temperature_k")
drg.add_variable("arch.habitat.cabin.total_pressure_kpa")

# Life support
drg.add_variable("arch.habitat.life_support.ogs.current_output_kg_s")
drg.add_variable("arch.habitat.life_support.sbar.current_removal_mol_s")
drg.add_variable("arch.habitat.life_support.sbar.sorbent_efficiency")
drg.add_variable("arch.habitat.life_support.phototherm.conversion_mol_s")
drg.add_variable("arch.habitat.life_support.phototherm.co_leakage_ppm")
drg.add_variable("arch.habitat.life_support.backup_o2.tank_mass_kg")
drg.add_variable("arch.habitat.life_support.backup_o2.days_of_backup")
drg.add_variable("arch.habitat.life_support.total_power_w")

# Power
drg.add_variable("arch.habitat.power.msmr.electrical_output_w")
drg.add_variable("arch.habitat.power.msmr.fuel_remaining_pct")
drg.add_variable("arch.habitat.power.fuel_cell.current_output_w")
drg.add_variable("arch.habitat.power.fuel_cell.h2_remaining_kg")
drg.add_variable("arch.habitat.power.battery.soc_pct")
drg.add_variable("arch.habitat.power.pmad.total_generation_w")
drg.add_variable("arch.habitat.power.pmad.total_load_w")
drg.add_variable("arch.habitat.power.pmad.load_shed_active")

# Thermal
drg.add_variable("arch.habitat.thermal.cabin_temp_k")
drg.add_variable("arch.habitat.thermal.radiator_rejection_w")
drg.add_variable("arch.habitat.thermal.heater_output_w")

# Communications
drg.add_variable("arch.habitat.comm.optical_link_up")
drg.add_variable("arch.habitat.comm.rf_link_up")
drg.add_variable("arch.habitat.comm.comm_blackout")
drg.add_variable("arch.habitat.comm.relay_elevation_deg")

# Structure
drg.add_variable("arch.habitat.structure.hoop_stress_mpa")
drg.add_variable("arch.habitat.structure.safety_factor")
drg.add_variable("arch.habitat.structure.radiation_dose_msv_day")

# Risk
drg.add_variable("arch.habitat.risk.crew_safety_score")
drg.add_variable("arch.habitat.risk.risk_level")
drg.add_variable("arch.habitat.risk.co2_warning_time_s")
drg.add_variable("arch.habitat.risk.o2_low_time_s")
drg.add_variable("arch.habitat.risk.o2_tank_margin_days")

trick.add_data_record_group(drg)

print("=" * 60)
print("  ARCH Habitat - Nominal 90-Day Mission")
print("  Crew: 6  |  Duration: 90 days  |  dt = 1.0 s")
print("  All subsystems nominal")
print("=" * 60)
