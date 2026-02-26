# ============================================================
#  ARCH_setup.ps1
#
#  Run this from inside the "files" folder:
#    C:\Users\bnard\Documents\files
#
#  It will:
#    1. Reorganize files into the Trick directory structure
#    2. Create the GitHub Actions workflow
#    3. Create a GitHub repo and push
#    4. Trigger the simulation to run in the cloud
#
#  Prerequisites (install these first if you haven't):
#    winget install Git.Git
#    winget install GitHub.cli
#  Then close and reopen PowerShell.
# ============================================================

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  ARCH Lunar Habitat - Full Setup Script" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

# ── Check prerequisites ──────────────────────────────────────

Write-Host "[Step 1] Checking prerequisites..." -ForegroundColor Yellow

try { git --version | Out-Null } catch {
    Write-Host "  ERROR: Git not found. Run: winget install Git.Git" -ForegroundColor Red
    Write-Host "  Then close and reopen PowerShell." -ForegroundColor Red
    exit 1
}
Write-Host "  Git: OK" -ForegroundColor Green

try { gh --version | Out-Null } catch {
    Write-Host "  ERROR: GitHub CLI not found. Run: winget install GitHub.cli" -ForegroundColor Red
    Write-Host "  Then close and reopen PowerShell." -ForegroundColor Red
    exit 1
}
Write-Host "  GitHub CLI: OK" -ForegroundColor Green

# Check if logged in
$authCheck = gh auth status 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "  You need to log in to GitHub first." -ForegroundColor Yellow
    gh auth login
}
Write-Host "  GitHub auth: OK" -ForegroundColor Green
Write-Host ""

# ── Reorganize into Trick directory structure ─────────────────

Write-Host "[Step 2] Reorganizing files into Trick structure..." -ForegroundColor Yellow

# Create directory structure
New-Item -ItemType Directory -Force -Path "models\habitat\include" | Out-Null
New-Item -ItemType Directory -Force -Path "models\habitat\src" | Out-Null
New-Item -ItemType Directory -Force -Path "RUN_nominal" | Out-Null
New-Item -ItemType Directory -Force -Path "RUN_failure" | Out-Null
New-Item -ItemType Directory -Force -Path ".github\workflows" | Out-Null
New-Item -ItemType Directory -Force -Path "docs" | Out-Null

# Move header file
if (Test-Path "habitat.h") {
    Move-Item -Force "habitat.h" "models\habitat\include\habitat.h"
    Write-Host "  Moved habitat.h -> models\habitat\include\" -ForegroundColor DarkGray
}

# Move source files
$sourceFiles = @(
    "habitat_init.c",
    "habitat_atmosphere.c",
    "habitat_power.c",
    "habitat_thermal.c",
    "habitat_comm.c",
    "habitat_structure.c",
    "habitat_risk.c"
)

foreach ($f in $sourceFiles) {
    if (Test-Path $f) {
        Move-Item -Force $f "models\habitat\src\$f"
        Write-Host "  Moved $f -> models\habitat\src\" -ForegroundColor DarkGray
    }
}

# Move input.py to RUN_nominal (this is the nominal run input)
if (Test-Path "input.py") {
    Copy-Item -Force "input.py" "RUN_nominal\input.py"
    Write-Host "  Copied input.py -> RUN_nominal\" -ForegroundColor DarkGray
}

# Move README
if (Test-Path "README.md") {
    Move-Item -Force "README.md" "docs\README.md"
    Write-Host "  Moved README.md -> docs\" -ForegroundColor DarkGray
}

# S_define and S_overrides.mk stay in the root (already there)
Write-Host "  S_define and S_overrides.mk: already in place" -ForegroundColor DarkGray

# Clean up the mnt folder and input.py copy if present
if (Test-Path "mnt") { Remove-Item -Recurse -Force "mnt" }
if (Test-Path "input.py") { Remove-Item -Force "input.py" }

Write-Host "  Directory structure created." -ForegroundColor Green
Write-Host ""

# ── Create the failure scenario input file ────────────────────

Write-Host "[Step 3] Creating failure scenario input file..." -ForegroundColor Yellow

$failureInput = @'
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
'@

Set-Content -Path "RUN_failure\input.py" -Value $failureInput
Write-Host "  RUN_failure\input.py created." -ForegroundColor Green
Write-Host ""

# ── Create GitHub Actions workflow ────────────────────────────

Write-Host "[Step 4] Creating GitHub Actions workflow..." -ForegroundColor Yellow

$workflow = @'
name: ARCH Habitat Simulation

on:
  push:
    branches: [main]
  workflow_dispatch:

jobs:
  build-and-run:
    runs-on: ubuntu-22.04
    timeout-minutes: 60

    steps:
      - name: Checkout ARCH Simulation
        uses: actions/checkout@v4

      - name: Install system dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            bison clang flex git llvm make maven swig cmake \
            curl g++ libx11-dev libxml2-dev libxt-dev \
            libmotif-common libmotif-dev \
            python3-dev zlib1g-dev llvm-dev libclang-dev \
            libudunits2-dev libgtest-dev default-jdk zip

      - name: Cache Trick installation
        id: cache-trick
        uses: actions/cache@v4
        with:
          path: ~/trick
          key: trick-${{ runner.os }}-v1

      - name: Build NASA Trick
        if: steps.cache-trick.outputs.cache-hit != 'true'
        run: |
          cd $HOME
          git clone https://github.com/nasa/trick.git
          cd trick
          export PYTHON_VERSION=3
          ./configure
          make -j$(nproc)

      - name: Configure environment
        run: |
          echo "TRICK_HOME=$HOME/trick" >> $GITHUB_ENV
          echo "$HOME/trick/bin" >> $GITHUB_PATH
          echo "PYTHON_VERSION=3" >> $GITHUB_ENV

      - name: Build ARCH simulation
        run: trick-CP

      - name: Run nominal mission
        run: ./S_main_*.exe RUN_nominal/input.py

      - name: Run OGS failure scenario
        run: ./S_main_*.exe RUN_failure/input.py

      - name: Upload nominal results
        uses: actions/upload-artifact@v4
        with:
          name: nominal-mission-results
          path: RUN_nominal/log_*/**
          if-no-files-found: warn

      - name: Upload failure results
        uses: actions/upload-artifact@v4
        with:
          name: ogs-failure-results
          path: RUN_failure/log_*/**
          if-no-files-found: warn

      - name: Print summary
        if: always()
        run: |
          echo "==== ARCH Simulation Complete ===="
          find RUN_nominal RUN_failure -name "*.csv" -o -name "*.trk" 2>/dev/null || echo "Check artifacts for results"
'@

Set-Content -Path ".github\workflows\sim.yml" -Value $workflow
Write-Host "  .github\workflows\sim.yml created." -ForegroundColor Green
Write-Host ""

# ── Create .gitignore ─────────────────────────────────────────

Write-Host "[Step 5] Creating .gitignore..." -ForegroundColor Yellow

$gitignore = @'
S_main_*.exe
S_source.hh
S_library_list
S_sie.resource
build/
trick/
*.o
*.d
*.pyc
__pycache__/
RUN_*/log_*/
RUN_*/send_hs*
.vscode/
.idea/
'@

Set-Content -Path ".gitignore" -Value $gitignore
Write-Host "  .gitignore created." -ForegroundColor Green
Write-Host ""

# ── Initialize git, create repo, push ─────────────────────────

Write-Host "[Step 6] Creating GitHub repository..." -ForegroundColor Yellow

git init 2>&1 | Out-Null
git add -A
git commit -m "ARCH lunar habitat Trick simulation" 2>&1 | Out-Null

$repoName = "ARCH-Trick-Simulation"

# Check if remote already exists
$remoteCheck = git remote get-url origin 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "  Remote already exists, pushing..." -ForegroundColor Yellow
    git branch -M main 2>&1 | Out-Null
    git push -u origin main --force 2>&1
} else {
    gh repo create $repoName --public --source=. --remote=origin --push 2>&1
}

Write-Host "  Code pushed to GitHub." -ForegroundColor Green
Write-Host ""

# ── Done ──────────────────────────────────────────────────────

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  DONE! The simulation is now running on GitHub." -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Watch the run live:" -ForegroundColor White
Write-Host "    gh run watch" -ForegroundColor Yellow
Write-Host ""
Write-Host "  Open in browser:" -ForegroundColor White
Write-Host "    gh repo view --web" -ForegroundColor Yellow
Write-Host "  Then click the Actions tab." -ForegroundColor White
Write-Host ""
Write-Host "  Download results when finished:" -ForegroundColor White
Write-Host "    gh run download" -ForegroundColor Yellow
Write-Host ""
Write-Host "  First run takes ~20-30 min (building Trick)." -ForegroundColor DarkGray
Write-Host "  Future runs will be ~5 min (cached)." -ForegroundColor DarkGray
Write-Host ""
