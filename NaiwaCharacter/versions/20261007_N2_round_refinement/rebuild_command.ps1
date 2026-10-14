# Run from any directory; create a separate rebuild instead of overwriting R1.
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
$parameterPath = Join-Path $PSScriptRoot 'candidate_R1/parameters.json'
$parameters = Get-Content -LiteralPath $parameterPath -Raw | ConvertFrom-Json
$destination = Join-Path $projectRoot ('NaiwaCharacter/versions/R1_rebuild_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
[System.IO.Directory]::CreateDirectory($destination) | Out-Null
$culture = [System.Globalization.CultureInfo]::InvariantCulture
$arguments = @('--output', $destination)
foreach ($pair in @(@('sigma_Z','sigma-z'), @('sigma_arc','sigma-arc'), @('front_inset','front-inset'), @('rear_fill','rear-fill'), @('iterations','iterations'), @('rho','rho'))) {
    $property = if ($pair[0] -eq 'sigma_Z') { 'sigma_z' } else { $pair[0] }
    $arguments += @(('--' + $pair[1]), ([Convert]::ToString($parameters.$property, $culture)))
}
Push-Location -LiteralPath $projectRoot
try {
    & (Join-Path $projectRoot 'NaiwaCharacter/run_blender_stage.ps1') -Script (Join-Path $projectRoot 'NaiwaCharacter/revise_n2_round_refinement.py') -Log (Join-Path $destination 'build.log') -ExpectedMarker 'N2_ROUND_REFINEMENT_BUILD_OK' -ScriptArgs $arguments
}
finally { Pop-Location }
