[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputFbx,
    [Parameter(Mandatory = $true)]
    [string]$OutputFbx,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$VsDevCmd = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
)

$ErrorActionPreference = 'Stop'
$inputPath = (Resolve-Path -LiteralPath $InputFbx).Path
$outputPath = [IO.Path]::GetFullPath($OutputFbx)
if ($inputPath -ieq $outputPath) { throw 'InputFbx and OutputFbx must be different files.' }
$outputDirectory = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$source = Join-Path $PSScriptRoot 'fbx_collapse_armature_wrapper.cpp'
$sdk = Join-Path $EngineRoot 'Engine/Source/ThirdParty/FBX/2020.2'
$libraryDirectory = Join-Path $sdk 'lib/vs2017/x64/release'
$includeDirectory = Join-Path $sdk 'include'
$dllDirectory = Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/FBX/2020.2/Win64'
$toolDirectory = Join-Path $env:TEMP 'NaiwaEggFbxRootWrapperTool'
New-Item -ItemType Directory -Path $toolDirectory -Force | Out-Null
$executable = Join-Path $toolDirectory 'fbx_collapse_armature_wrapper.exe'
$objectFile = Join-Path $toolDirectory 'fbx_collapse_armature_wrapper.obj'
$batchFile = Join-Path $toolDirectory 'build_and_run.cmd'

foreach ($required in @($source, $VsDevCmd, $libraryDirectory, $includeDirectory, $dllDirectory)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required FBX tool input not found: $required" }
}

$batch = @"
@echo off
call "$VsDevCmd" -arch=x64
if errorlevel 1 exit /b 11
cl /nologo /EHsc /MD /std:c++17 /D FBXSDK_SHARED /I "$includeDirectory" /I "$includeDirectory\fbxsdk" "$source" /Fo:"$objectFile" /Fe:"$executable" /link /LIBPATH:"$libraryDirectory" libfbxsdk.lib
if errorlevel 1 exit /b 12
set "PATH=$dllDirectory;%PATH%"
"$executable" "$inputPath" "$outputPath"
"@
[IO.File]::WriteAllText($batchFile, $batch, [Text.Encoding]::ASCII)

& cmd.exe /d /c $batchFile
if ($LASTEXITCODE -ne 0) { throw "FBX root-wrapper collapse failed with exit code $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $outputPath) -or (Get-Item -LiteralPath $outputPath).Length -lt 4096) {
    throw "FBX root-wrapper tool did not produce a valid output: $outputPath"
}
Write-Output "Wrote root-only FBX: $outputPath"
