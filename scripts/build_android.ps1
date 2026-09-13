<#
.SYNOPSIS
    OpenXRay Android ARM64 Build Entry-Point (PowerShell)
.DESCRIPTION
    Cross-compiles OpenXRay for Android ARM64 (arm64-v8a) targeting Android API 29+
    using Android NDK Clang and Ninja.
.PARAMETER Config
    Build configuration: Release (default) or Debug.
.PARAMETER NdkPath
    Custom path to Android NDK. If omitted, checks $env:ANDROID_NDK_HOME,
    $env:ANDROID_NDK_ROOT, and common system locations.
.PARAMETER Clean
    Perform clean build by deleting the preset binary directory first.
.PARAMETER ConfigureOnly
    Run CMake configure without building.
.PARAMETER BuildOnly
    Run CMake build without re-configuring.
.PARAMETER Target
    Specific CMake build target.
#>

[CmdletBinding()]
param (
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",

    [string]$NdkPath = "",

    [switch]$Clean,
    [switch]$ConfigureOnly,
    [switch]$BuildOnly,
    [Alias("Targets")]
    [string[]]$Target = @(),
    [switch]$Help
)

if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 0
}

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path "$PSScriptRoot\..").Path

Write-Host "=== OpenXRay Android ARM64 Build Entry-Point ===" -ForegroundColor Cyan
Write-Host "Project Root : $ProjectRoot"
Write-Host "Config       : $Config"
Write-Host "Target ABI   : arm64-v8a"
Write-Host "Target API   : 29 (Android 10)"

# 1. Resolve Android NDK
if (-not [string]::IsNullOrWhiteSpace($NdkPath)) {
    if (Test-Path "$NdkPath\build\cmake\android.toolchain.cmake") {
        $env:ANDROID_NDK_HOME = (Resolve-Path $NdkPath).Path
    } else {
        Write-Error "Provided -NdkPath '$NdkPath' is invalid or missing 'build\cmake\android.toolchain.cmake'."
        exit 1
    }
}

if ([string]::IsNullOrWhiteSpace($env:ANDROID_NDK_HOME)) {
    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_NDK_ROOT) -and (Test-Path "$env:ANDROID_NDK_ROOT\build\cmake\android.toolchain.cmake")) {
        $env:ANDROID_NDK_HOME = $env:ANDROID_NDK_ROOT
    } elseif (-not [string]::IsNullOrWhiteSpace($env:ANDROID_NDK) -and (Test-Path "$env:ANDROID_NDK\build\cmake\android.toolchain.cmake")) {
        $env:ANDROID_NDK_HOME = $env:ANDROID_NDK
    }
}

# Auto-probe common NDK locations if still unset
if ([string]::IsNullOrWhiteSpace($env:ANDROID_NDK_HOME)) {
    $candidatePaths = @(
        "C:\Program Files\Unity\Hub\Editor\2023.2.2f1\Editor\Data\PlaybackEngines\AndroidPlayer\NDK",
        "$env:LOCALAPPDATA\Android\Sdk\ndk\* | Sort-Object -Descending | Select-Object -First 1",
        "C:\Android\ndk\*"
    )
    foreach ($cand in $candidatePaths) {
        $resolved = Get-Item $cand -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($resolved -and (Test-Path "$($resolved.FullName)\build\cmake\android.toolchain.cmake")) {
            $env:ANDROID_NDK_HOME = $resolved.FullName
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($env:ANDROID_NDK_HOME) -or -not (Test-Path "$env:ANDROID_NDK_HOME\build\cmake\android.toolchain.cmake")) {
    Write-Error "Android NDK not found. Please set ANDROID_NDK_HOME or pass -NdkPath <path_to_ndk>."
    exit 1
}

$ToolchainFile = "$env:ANDROID_NDK_HOME\build\cmake\android.toolchain.cmake"
Write-Host "Android NDK  : $env:ANDROID_NDK_HOME" -ForegroundColor Green
Write-Host "Toolchain    : $ToolchainFile"

# 2. Check for CMake and Ninja
$NinjaCmd = Get-Command "ninja" -ErrorAction SilentlyContinue
if (-not $NinjaCmd) {
    $ninjaWinget = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter "ninja.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($ninjaWinget) {
        $ninjaDir = $ninjaWinget.DirectoryName
        $env:PATH = "$ninjaDir;$env:PATH"
        $NinjaCmd = Get-Command "ninja" -ErrorAction SilentlyContinue
        Write-Host "Discovered Ninja at: $ninjaDir" -ForegroundColor Gray
    }
}
if (-not $NinjaCmd) {
    Write-Warning "Ninja executable was not found in PATH. Ensure Ninja is installed and discoverable."
}

$CMakeCmd = Get-Command "cmake" -ErrorAction SilentlyContinue
if (-not $CMakeCmd) {
    $cmakePaths = @(
        "$env:LOCALAPPDATA\Microsoft\WinGet\Links",
        "C:\Program Files\CMake\bin",
        "$env:LOCALAPPDATA\Android\Sdk\cmake\*\bin"
    )
    foreach ($cp in $cmakePaths) {
        $found = Get-Item "$cp\cmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            $env:PATH = "$($found.DirectoryName);$env:PATH"
            $CMakeCmd = Get-Command "cmake" -ErrorAction SilentlyContinue
            Write-Host "Discovered CMake at: $($found.DirectoryName)" -ForegroundColor Gray
            break
        }
    }
}
if (-not $CMakeCmd) {
    Write-Error "CMake executable was not found in PATH. Please install CMake 3.23+."
    exit 1
}

# 3. Clean if requested
$PresetName = "android-arm64-$($Config.ToLower())"
$BuildDir = "$ProjectRoot\build\$PresetName"

if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Host "Cleaning build directory: $BuildDir" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }
}

# 4. Configure
Push-Location $ProjectRoot
try {
    if (-not $BuildOnly) {
        Write-Host "`n--- Configuring with preset: $PresetName ---" -ForegroundColor Cyan
        & cmake --preset $PresetName
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake configuration failed with exit code $LASTEXITCODE."
            exit $LASTEXITCODE
        }
    }

    # 5. Build
    if (-not $ConfigureOnly) {
        Write-Host "`n--- Building preset: $PresetName ---" -ForegroundColor Cyan
        $buildArgs = @("--build", "--preset", $PresetName)
        if ($Target.Count -gt 0) {
            foreach ($t in $Target) {
                if (-not [string]::IsNullOrWhiteSpace($t)) {
                    $buildArgs += @("--target", $t)
                }
            }
        }
        & cmake @buildArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake build failed with exit code $LASTEXITCODE."
            exit $LASTEXITCODE
        }
    }
}
finally {
    Pop-Location
}

Write-Host "`n[SUCCESS] Android build step completed." -ForegroundColor Green
