# fix_co2.ps1 - Fix CO2 removal rate
# The SBAR must be sized to handle all crew CO2 on its own.
# 6 crew * 0.000012731 kg/s / 0.044 kg/mol = 0.001736 mol/s
# We size SBAR at 120% margin = 0.002083 mol/s

$ErrorActionPreference = "Stop"

Write-Host "Fixing CO2 removal rate..." -ForegroundColor Cyan

$initPath = "models\habitat\src\habitat_init.c"
$content = Get-Content $initPath -Raw

# Increase SBAR max removal to 120% of crew production
$content = $content -replace "max_removal_mol_s    = 0\.001578", "max_removal_mol_s    = 0.002083"
$content = $content -replace "current_removal_mol_s = 0\.001578", "current_removal_mol_s = 0.002083"

Set-Content -Path $initPath -Value $content -NoNewline

Write-Host "  SBAR removal rate: 0.001578 -> 0.002083 mol/s (120% of crew load)" -ForegroundColor Green

git add -A
git commit -m "Fix CO2: size SBAR at 120% crew production rate"
git push

Write-Host ""
Write-Host "Pushed! Watch with: gh run watch" -ForegroundColor Yellow
