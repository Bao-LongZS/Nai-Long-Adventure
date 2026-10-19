[CmdletBinding()]
param(
    [string]$Repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path,
    [string]$RunId = '20261013T053123Z_44908'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath $Repository).Path
$outDir = Join-Path $repo 'UE5/Records/p1_20261015_baseline'
$zipPath = Join-Path $outDir 'p0_before_p1.zip'
$manifestPath = Join-Path $outDir 'p0_before_p1_manifest.json'
if (Test-Path -LiteralPath $zipPath) { throw "Snapshot exists; refusing overwrite: $zipPath" }
if (Test-Path -LiteralPath $manifestPath) { throw "Manifest exists; refusing overwrite: $manifestPath" }
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

$directories = @(
    'UE5/NaiwaP0/Config',
    'UE5/NaiwaP0/Source',
    'UE5/NaiwaP0/Content',
    'UE5/Scripts',
    'UE5/Records/p0_completion_20261013',
    "UE5/NaiwaP0/Saved/NaiwaTests/$RunId"
)
$individualFiles = @(
    'UE5/NaiwaP0/Nai_Long.uproject',
    'UE5/launch_p0.ps1',
    'UE5/run_stage.ps1',
    'UE5/Records/P0_CHECKPOINT_20261013.md',
    'UE5/Records/TASK_STATE.json'
)
$selected = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
foreach ($relative in $directories) {
    $absolute = Join-Path $repo $relative
    if (-not (Test-Path -LiteralPath $absolute -PathType Container)) { throw "Missing directory: $absolute" }
    foreach ($file in Get-ChildItem -LiteralPath $absolute -Recurse -File) {
        if ($file.FullName -notmatch '[\\/]__pycache__[\\/]' -and $file.Extension -ne '.pyc') { $selected.Add($file) }
    }
}
foreach ($relative in $individualFiles) {
    $absolute = Join-Path $repo $relative
    if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) { throw "Missing file: $absolute" }
    $selected.Add((Get-Item -LiteralPath $absolute))
}

$selected = @($selected | Sort-Object FullName -Unique)
$head = (& git -C $repo rev-parse HEAD).Trim()
$gitStatus = @(& git -C $repo -c core.quotepath=false status --porcelain=v1)
$archive = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
$entries = [System.Collections.Generic.List[object]]::new()
try {
    foreach ($file in $selected) {
        $relative = $file.FullName.Substring($repo.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, $file.FullName, $relative, [System.IO.Compression.CompressionLevel]::Fastest)
        $entries.Add([ordered]@{ path = $relative; bytes = $file.Length; sha256 = $hash })
    }
} finally { $archive.Dispose() }

$badEntry = $null
$checkArchive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    if ($checkArchive.Entries.Count -ne $entries.Count) { throw 'ZIP entry count mismatch' }
    foreach ($entry in $checkArchive.Entries) {
        if ($entry.Length -lt 0) { $badEntry = $entry.FullName; break }
    }
} finally { $checkArchive.Dispose() }
if ($badEntry) { throw "Invalid ZIP entry: $badEntry" }

$manifest = [ordered]@{
    created_utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    purpose = 'Read-only snapshot of delivered P0 before P1 modifications; engine-generated Binaries/Intermediate/DerivedDataCache excluded.'
    repository_head = $head
    repository_status_before_capture = $gitStatus
    project = 'UE5/NaiwaP0/Nai_Long.uproject'
    level = 'UE5/NaiwaP0/Content/Naiwa/Maps/L_NaiwaGraybox.umap'
    engine = '5.8.3-58210709'
    p0_final_test_run = $RunId
    new_process_p0_launch = 'pending_reliable_verification'
    archive = 'p0_before_p1.zip'
    archive_bytes = (Get-Item -LiteralPath $zipPath).Length
    archive_sha256 = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    file_count = $entries.Count
    files = $entries
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Output ([pscustomobject]@{
    Zip = $zipPath
    Manifest = $manifestPath
    Files = $entries.Count
    ZipMB = [math]::Round($manifest.archive_bytes / 1MB, 2)
    SHA256 = $manifest.archive_sha256
})
