# ============================================================
#  fix_and_push.ps1
#
#  Fixes the Trick build errors:
#    1. Adds LIBRARY DEPENDENCY block to habitat.h
#    2. Fixes #include paths in all .c source files
#    3. Commits and pushes to trigger a new build
#
#  Run from: C:\Users\bnard\Documents\files
# ============================================================

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "Fixing ARCH simulation build errors..." -ForegroundColor Cyan
Write-Host ""

# ── Fix 1: Replace habitat.h with corrected version ──────────

Write-Host "[Fix 1] Adding LIBRARY DEPENDENCY to habitat.h..." -ForegroundColor Yellow

$headerPath = "models\habitat\include\habitat.h"
$headerContent = Get-Content $headerPath -Raw

# Check if already fixed
if ($headerContent -match "LIBRARY DEPENDENCY") {
    Write-Host "  Already fixed." -ForegroundColor Green
} else {
    # Insert the LIBRARY DEPENDENCY block right before #ifndef HABITAT_H
    $libraryBlock = @"

/*
PURPOSE: ARCH Habitat model data structures and prototypes.

LIBRARY DEPENDENCY:
    ((habitat/src/habitat_init.c)
     (habitat/src/habitat_atmosphere.c)
     (habitat/src/habitat_power.c)
     (habitat/src/habitat_thermal.c)
     (habitat/src/habitat_comm.c)
     (habitat/src/habitat_structure.c)
     (habitat/src/habitat_risk.c))
*/

"@

    $headerContent = $headerContent -replace "#ifndef HABITAT_H", "$libraryBlock#ifndef HABITAT_H"
    Set-Content -Path $headerPath -Value $headerContent -NoNewline
    Write-Host "  LIBRARY DEPENDENCY block added." -ForegroundColor Green
}

Write-Host ""

# ── Fix 2: Fix #include paths in all .c source files ─────────

Write-Host "[Fix 2] Fixing #include paths in source files..." -ForegroundColor Yellow

$srcFiles = Get-ChildItem "models\habitat\src\*.c"
foreach ($file in $srcFiles) {
    $content = Get-Content $file.FullName -Raw
    if ($content -match '"../include/habitat.h"') {
        $content = $content -replace '"../include/habitat.h"', '"habitat/include/habitat.h"'
        Set-Content -Path $file.FullName -Value $content -NoNewline
        Write-Host "  Fixed: $($file.Name)" -ForegroundColor DarkGray
    } else {
        Write-Host "  Already correct: $($file.Name)" -ForegroundColor DarkGray
    }
}

Write-Host "  All include paths fixed." -ForegroundColor Green
Write-Host ""

# ── Commit and push ──────────────────────────────────────────

Write-Host "[Pushing fix to GitHub...]" -ForegroundColor Yellow

git add -A
git commit -m "Fix: add LIBRARY DEPENDENCY and correct include paths"
git push

Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  Fix pushed! A new build is starting on GitHub." -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Watch it with:  gh run watch" -ForegroundColor Yellow
Write-Host ""
