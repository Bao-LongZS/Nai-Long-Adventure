[CmdletBinding()]
param([string]$Repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath $Repository).Path
$source = Join-Path $repo 'out/build/x64-Release'
$comparison = Join-Path $repo 'out/build/x64-Debug'
$destination = Join-Path $repo 'UE5/SourceAssets/SFML_Level1'
$manifestPath = Join-Path $destination 'asset_manifest.json'
$application = Join-Path $repo 'Application.cpp'

$activePng = @(
    'player.png', 'Glance.png', 'Big_Laugh.png', 'collection1.png', 'collection2.png',
    'Spine.png', 'Tree.png', 'Leave_Left.png', 'Leave_Right.png', 'Cloud_Mass.png',
    'Snow.png', 'Snow_Ground1.png', 'Snow_Ground2.png', 'Snow_Ground3.png'
)
$loadedOnlyPng = @('Desert.png', 'Forest.png')
$activeWav = @(
    'Collection.wav', 'Die.wav', 'Die_1.wav', 'Jump.wav',
    'Background_Music_1.wav', 'Background_Music_2.wav',
    'Background_Music_3.wav', 'Background_Music_4.wav',
    'Collection_1.wav', 'Die_3.wav'
)
$loadedOnlyWav = @('Collection_2.wav', 'Hurdle2_1.wav', 'Hurdle2_2.wav')
$all = @($activePng + $loadedOnlyPng + $activeWav + $loadedOnlyWav)
if ($all.Count -ne 29 -or (@($all | Select-Object -Unique).Count -ne $all.Count)) {
    throw 'Expected 29 unique Application.cpp PNG/WAV dependencies'
}

$applicationText = Get-Content -LiteralPath $application -Raw
$referenced = @([regex]::Matches($applicationText, '"([^"\r\n]+\.(?:png|wav))"') |
    ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
$missingFromInventory = @($referenced | Where-Object { $_ -cnotin $all })
$notReferenced = @($all | Where-Object { $_ -cnotin $referenced })
if ($missingFromInventory.Count -gt 0 -or $notReferenced.Count -gt 0) {
    throw "Application.cpp dependency inventory changed. Missing: $($missingFromInventory -join ', '); obsolete: $($notReferenced -join ', ')"
}

foreach ($directory in @($destination, (Join-Path $destination 'png'), (Join-Path $destination 'wav'))) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
$entries = [System.Collections.Generic.List[object]]::new()
foreach ($name in ($all | Sort-Object)) {
    $kind = [System.IO.Path]::GetExtension($name).TrimStart('.').ToLowerInvariant()
    $releaseFile = Join-Path $source $name
    $debugFile = Join-Path $comparison $name
    $destFile = Join-Path (Join-Path $destination $kind) $name
    if (-not (Test-Path -LiteralPath $releaseFile -PathType Leaf)) { throw "Missing Release asset: $releaseFile" }
    if (-not (Test-Path -LiteralPath $debugFile -PathType Leaf)) { throw "Missing Debug asset: $debugFile" }
    $releaseHash = (Get-FileHash -LiteralPath $releaseFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $debugHash = (Get-FileHash -LiteralPath $debugFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($releaseHash -ne $debugHash) { throw "Release and Debug assets differ: $name" }
    if (Test-Path -LiteralPath $destFile) {
        $existingHash = (Get-FileHash -LiteralPath $destFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($existingHash -ne $releaseHash) { throw "Destination differs; refusing overwrite: $destFile" }
    } else {
        Copy-Item -LiteralPath $releaseFile -Destination $destFile
    }
    $copiedHash = (Get-FileHash -LiteralPath $destFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($copiedHash -ne $releaseHash) { throw "Copied asset failed SHA256 check: $name" }
    $role = if ($name -cin $activePng -or $name -cin $activeWav) { 'active_in_first_level' } else { 'loaded_by_constructor_only' }
    $entries.Add([ordered]@{
        name = $name
        role = $role
        source_release = "out/build/x64-Release/$name"
        source_debug = "out/build/x64-Debug/$name"
        formal_source = "UE5/SourceAssets/SFML_Level1/$kind/$name"
        bytes = (Get-Item -LiteralPath $destFile).Length
        sha256 = $copiedHash
        release_debug_hash_equal = $true
    })
}
$manifest = [ordered]@{
    created_utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    source_code = 'Application.cpp'
    rationale = 'All 29 PNG/WAV paths are loaded by the first-level Application constructor. Active assets are rendered or played by the current code; loaded-only assets are still required to instantiate the old app without missing files.'
    active_count = @($entries | Where-Object role -eq 'active_in_first_level').Count
    loaded_only_count = @($entries | Where-Object role -eq 'loaded_by_constructor_only').Count
    count = $entries.Count
    assets = $entries
}
$manifest | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Output ([pscustomobject]@{
    Manifest = $manifestPath
    Files = $entries.Count
    Active = $manifest.active_count
    LoadedOnly = $manifest.loaded_only_count
    Bytes = (@($entries | ForEach-Object { [long]$_['bytes'] }) | Measure-Object -Sum).Sum
})
