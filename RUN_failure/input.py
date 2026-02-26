# RUN_failure/input.py - ARCH Habitat OGS Failure Scenario
# OGS fails at day 30, testing backup O2 systems

mission_duration = 90 * 24 * 3600
trick.exec_set_terminate_time(mission_duration)
trick.real_time_disable()

arch.habitat.crew_count           = 6
arch.habitat.crew_activity_factor = 1.0
arch.habitat.power.msmr.is_active = 1
arch.habitat.life_support.ogs.is_active        = 1
arch.habitat.life_support.sbar.is_active       = 1
arch.habitat.life_support.phototherm.is_active = 1

# OGS failure at day 30
ogs_failure_time = 30 * 24 * 3600
ogs_fail_event = trick.new_event("OGS_Failure")
ogs_fail_event.condition(0, "arch.habitat.mission_elapsed_time_s >= %d" % ogs_failure_time)
ogs_fail_event.action(0, "arch.habitat.life_support.ogs.has_fault = 1")
ogs_fail_event.action(1, "arch.habitat.life_support.ogs.is_active = 0")
ogs_fail_event.action(2, "print('*** EVENT: OGS PRIMARY FAILURE at day 30 ***')")
ogs_fail_event.activate()
trick.add_event(ogs_fail_event)

drg = trick.DRAscii("ARCH_OGS_Failure")
drg.set_cycle(60.0)
drg.set_freq(trick.DR_Always)
drg.add_variable("arch.habitat.mission_elapsed_time_s")
drg.add_variable("arch.habitat.mission_day")
drg.add_variable("arch.habitat.cabin.co2_ppm")
drg.add_variable("arch.habitat.cabin.o2_pct")
drg.add_variable("arch.habitat.cabin.temperature_k")
drg.add_variable("arch.habitat.life_support.ogs.is_active")
drg.add_variable("arch.habitat.life_support.backup_o2.tank_mass_kg")
drg.add_variable("arch.habitat.life_support.backup_o2.days_of_backup")
drg.add_variable("arch.habitat.power.battery.soc_pct")
drg.add_variable("arch.habitat.risk.crew_safety_score")
drg.add_variable("arch.habitat.risk.risk_level")
trick.add_data_record_group(drg)

print("=" * 60)
print("  ARCH Habitat - OGS Failure Scenario")
print("  OGS fails at day 30 -> backup O2 + photothermal only")
print("=" * 60)
