# Independently rebuild H2, then E2 from the newly saved H2; preserve deliveries.
[CmdletBinding()]
param(
    [string]$Output,
    [string]$Blender = 'D:\STEAM\steamapps\common\Blender\blender.exe'
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
if (-not $Output) {
    $Output = Join-Path $PSScriptRoot ('rebuild_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
}
$destination = [System.IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $destination) { throw 'Rebuild output must be a new directory.' }
[System.IO.Directory]::CreateDirectory($destination) | Out-Null
$stage = Join-Path $projectRoot 'NaiwaCharacter/run_blender_stage.ps1'
$culture = [System.Globalization.CultureInfo]::InvariantCulture
# The saved build options are the actual recipe. Forward each option once,
# including future geometry options, instead of silently rebuilding an old recipe.
function Convert-OptionsToArguments($Options) {
    $arguments = @()
    foreach ($property in $Options.PSObject.Properties) {
        $flag = '--' + $property.Name.Replace('_', '-')
        if ($property.Value -is [bool]) {
            if ($property.Value) { $arguments += $flag }
        } elseif ($null -eq $property.Value -or $property.Value -is [System.Array]) {
            throw "Build option '$($property.Name)' must be a scalar or Boolean."
        } else {
            $arguments += @($flag, [Convert]::ToString($property.Value, $culture))
        }
    }
    return $arguments
}
$fixedR1 = Join-Path $projectRoot 'NaiwaCharacter/versions/20261007_N2_round_refinement/candidate_R1/Naiwa_Character.blend'
$fixedHash = 'ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24'
if ((Get-FileHash -LiteralPath $fixedR1 -Algorithm SHA256).Hash.ToLowerInvariant() -ne $fixedHash) {
    throw 'Fixed R1 input hash differs.'
}
$h2 = Join-Path $destination 'candidate_H2'
$h2Parameters = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'candidate_H2/parameters.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$h2Args = @('--output', $h2) + @(Convert-OptionsToArguments $h2Parameters)
& $stage -Blender $Blender -Script (Join-Path $projectRoot 'NaiwaCharacter/revise_h2_overall_refinement.py') -Log (Join-Path $destination 'build_H2.log') -ExpectedMarker 'H2_NATIVE_BUILD_OK' -ScriptArgs $h2Args
$h2Native = Join-Path $h2 'Naiwa_Character.blend'
$h2Hash = (Get-FileHash -LiteralPath $h2Native -Algorithm SHA256).Hash.ToLowerInvariant()
$e2Parameters = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'candidate_E2/e2_build_report.json') -Raw -Encoding UTF8 | ConvertFrom-Json).options
$e2 = Join-Path $destination 'candidate_E2'
$e2Args = @('--source', $h2Native, '--source-sha', $h2Hash, '--output', $e2) + @(Convert-OptionsToArguments $e2Parameters)
& $stage -Blender $Blender -Script (Join-Path $projectRoot 'NaiwaCharacter/revise_e2_eye_refinement.py') -Log (Join-Path $destination 'build_E2.log') -ExpectedMarker 'E2_EYE_REFINEMENT_BUILD_OK' -ScriptArgs $e2Args
foreach ($label in @('H2','E2')) {
    $native = Join-Path $destination ('candidate_' + $label + '/Naiwa_Character.blend')
    $target = Join-Path $PSScriptRoot ('candidate_' + $label + '/Naiwa_Character.blend')
    $report = Join-Path $destination ('candidate_' + $label + '/rebuild_validation.json')
    & $stage -Blender $Blender -Script (Join-Path $projectRoot 'NaiwaCharacter/validate_delivery_rebuild.py') -Log (Join-Path $destination ('rebuild_' + $label + '.log')) -ExpectedMarker 'DELIVERY_REBUILD_INDEPENDENT_MATCH_OK' -ScriptArgs @('--target', $target, '--rebuild', $native, '--output', $report)
    & $stage -Blender $Blender -Script (Join-Path $projectRoot 'NaiwaCharacter/export_saved_fbx.py') -Log (Join-Path $destination ('export_' + $label + '.log')) -ExpectedMarker 'SAVED_FBX_EXPORT_OK' -ScriptArgs @('--blend', $native, '--bake-step', '0.25', '--bake-simplify', '0')
    & $stage -Blender $Blender -Script (Join-Path $projectRoot 'NaiwaCharacter/validate_h2_e2_fbx.py') -Log (Join-Path $destination ('fbx_reimport_' + $label + '.log')) -ExpectedMarker 'H2_E2_FBX_SURFACE_REIMPORT_OK' -ScriptArgs @('--blend', $native, '--fbx', ([System.IO.Path]::ChangeExtension($native, '.fbx')), '--label', $label)
}
if ((Get-FileHash -LiteralPath $fixedR1 -Algorithm SHA256).Hash.ToLowerInvariant() -ne $fixedHash) {
    throw 'Fixed R1 changed during rebuild.'
}
Write-Output "H2_E2_INDEPENDENT_REBUILD_OK $destination"
