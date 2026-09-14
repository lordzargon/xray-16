<#
.SYNOPSIS
    Deploys S.T.A.L.K.E.R. Call of Pripyat assets and OpenXRay shaders to an Android device.

.DESCRIPTION
    Validates a local S.T.A.L.K.E.R. Call of Pripyat installation (such as GOG),
    generates an Android-tailored fsgame.ltx, copies OpenXRay OpenGL shaders,
    and transfers all required archives and directories to the Android device using adb.
    Optionally installs the built APK and launches the game.

.PARAMETER SourcePath
    Path to the Call of Pripyat game root directory.
    Default: "C:\Program Files (x86)\GOG Galaxy\Games\S.T.A.L.K.E.R. Call of Pripyat"

.PARAMETER Destination
    Target storage directory on Android device.
    Default: "/sdcard/Android/data/org.openxray/files"

.PARAMETER AdbPath
    Optional explicit path to adb.exe.

.PARAMETER InstallApk
    Installs the debug APK (android/app/build/outputs/apk/debug/app-debug.apk) after pushing data.

.PARAMETER LaunchApp
    Launches the OpenXRay activity via adb shell after deployment.

.PARAMETER DryRun
    Displays detected assets and planned actions without executing transfers.
#>

[CmdletBinding()]
param(
    [string]$SourcePath = "C:\Program Files (x86)\GOG Galaxy\Games\S.T.A.L.K.E.R. Call of Pripyat",
    [string]$Destination = "/sdcard/Android/data/org.openxray/files",
    [string]$AdbPath = "",
    [switch]$InstallApk,
    [switch]$LaunchApp,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

Write-Host "=== OpenXRay Android Asset Deployment Pipeline ===" -ForegroundColor Cyan
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Write-Host "Project Root : $ProjectRoot"
Write-Host "Source Path  : $SourcePath"
Write-Host "Destination  : $Destination"

# 1. Validate Source Game Installation
if (-not (Test-Path $SourcePath)) {
    Write-Error "Call of Pripyat source folder not found at: '$SourcePath'"
    exit 1
}

$requiredDirs = @("resources", "levels", "localization", "patches")
foreach ($dir in $requiredDirs) {
    $fullDir = Join-Path $SourcePath $dir
    if (-not (Test-Path $fullDir)) {
        Write-Error "Missing required game subdirectory: '$fullDir'"
        exit 1
    }
}

$resourcesDb = Get-ChildItem -Path (Join-Path $SourcePath "resources") -Filter "*.db*"
Write-Host "Detected $($resourcesDb.Count) resource database files in resources/" -ForegroundColor Green

# 2. Locate adb.exe
if (-not $AdbPath) {
    if (Get-Command adb -ErrorAction SilentlyContinue) {
        $AdbPath = (Get-Command adb).Source
    } else {
        $candidatePaths = @(
            "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
            "C:\Program Files\Unity\Hub\Editor\2023.2.2f1\Editor\Data\PlaybackEngines\AndroidPlayer\SDK\platform-tools\adb.exe"
        )
        foreach ($cand in $candidatePaths) {
            if (Test-Path $cand) {
                $AdbPath = $cand
                break
            }
        }
    }
}

if (-not $AdbPath -or -not (Test-Path $AdbPath)) {
    Write-Error "adb.exe not found. Please specify -AdbPath or ensure Android SDK platform-tools is installed."
    exit 1
}

Write-Host "Using ADB    : $AdbPath" -ForegroundColor Green

# 3. Check Connected Devices
if (-not $DryRun) {
    $devices = & $AdbPath devices | Where-Object { $_ -match "\bdevice\b" -and $_ -notmatch "List of devices" }
    if (-not $devices) {
        Write-Warning "No Android device currently detected by ADB. Please connect a device with USB Debugging enabled."
        Write-Host "Run 'adb devices' to confirm connection."
        exit 1
    }
    Write-Host "Connected Device(s):" -ForegroundColor Green
    $devices | ForEach-Object { Write-Host "  - $_" }
}

# 4. Generate Android fsgame.ltx
$tempDir = Join-Path $ProjectRoot "build\android-temp"
if (-not (Test-Path $tempDir)) { New-Item -ItemType Directory -Path $tempDir -Force | Out-Null }
$androidFsgame = Join-Path $tempDir "fsgame.ltx"

$fsgameContent = @'
; OpenXRay Android Filesystem Configuration
$app_data_root$         = true | false| $fs_root$      | appdata/
$arch_dir$              = false| false| $fs_root$
$game_arch_mp$          = false| false| $fs_root$      | mp/
$arch_dir_levels$       = false| false| $fs_root$      | levels/
$arch_dir_resources$    = false| false| $fs_root$      | resources/
$arch_dir_localization$ = false| false| $fs_root$      | localization/
$arch_dir_patches$      = false| true | $fs_root$      | patches/
$game_data$             = false| true | $fs_root$      | gamedata/
$game_ai$               = true | false| $game_data$    | ai/
$game_spawn$            = true | false| $game_data$    | spawns/
$game_levels$           = true | false| $game_data$    | levels/
$game_meshes$           = true | true | $game_data$    | meshes/ | *.ogf;*.omf | Game Object files
$game_anims$            = true | true | $game_data$    | anims/  | *.anm;*.anms| Animation files
$game_dm$               = true | true | $game_data$    | meshes/ | *.dm        | Detail Model files
$game_shaders$          = true | true | $game_data$    | shaders/
$game_sounds$           = true | true | $game_data$    | sounds/
$game_textures$         = true | true | $game_data$    | textures/
$game_config$           = true | false| $game_data$    | configs/
$game_weathers$         = true | false| $game_config$  | environment/weathers/
$game_weather_effects$  = true | false| $game_config$  | environment/weather_effects/
$textures$              = true | true | $game_data$    | textures/
$level$                 = false| false| $game_levels$
$game_scripts$          = true | false| $game_data$    | scripts/| *.script    | Game script files
$logs$                  = true | false| $app_data_root$| logs/
$screenshots$           = true | false| $app_data_root$| screenshots/
$game_saves$            = true | false| $app_data_root$| savedgames/
$downloads$             = false| false| $app_data_root$
'@
Set-Content -Path $androidFsgame -Value $fsgameContent -Encoding UTF8
Write-Host "Generated Android fsgame.ltx at: $androidFsgame" -ForegroundColor Green

if ($DryRun) {
    Write-Host "[DRY RUN] All prerequisites validated successfully. Ready to push to $Destination." -ForegroundColor Yellow
    exit 0
}

# 5. Push Data to Device
Write-Host "Creating target directories on device..." -ForegroundColor Cyan
& $AdbPath shell "mkdir -p $Destination/gamedata/shaders"
& $AdbPath shell "mkdir -p $Destination/appdata"

Write-Host "Pushing Android fsgame.ltx..." -ForegroundColor Cyan
& $AdbPath push "$androidFsgame" "$Destination/fsgame.ltx"

Write-Host "Pushing OpenXRay OpenGL shaders..." -ForegroundColor Cyan
$shadersGl = Join-Path $ProjectRoot "res\gamedata\shaders\gl"
if (Test-Path $shadersGl) {
    & $AdbPath push "$shadersGl" "$Destination/gamedata/shaders/"
}

$dirsToPush = @("resources", "levels", "localization", "patches")
$optionalDirs = @("mp")
foreach ($opt in $optionalDirs) {
    if (Test-Path (Join-Path $SourcePath $opt)) { $dirsToPush += $opt }
}

foreach ($d in $dirsToPush) {
    $src = Join-Path $SourcePath $d
    Write-Host "Pushing $d from $src to $Destination/$d/..." -ForegroundColor Cyan
    & $AdbPath push "$src" "$Destination/"
}

# 6. Optional APK Installation
if ($InstallApk) {
    $apkPath = Join-Path $ProjectRoot "android\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apkPath)) {
        Write-Warning "APK not found at '$apkPath'. Run Gradle build first."
    } else {
        Write-Host "Installing OpenXRay APK: $apkPath..." -ForegroundColor Cyan
        & $AdbPath install -r "$apkPath"
    }
}

# 7. Optional Launch
if ($LaunchApp) {
    Write-Host "Launching OpenXRay..." -ForegroundColor Cyan
    & $AdbPath shell am start -n org.openxray/.OpenXRayActivity
}

Write-Host "=== Deployment Completed Successfully! ===" -ForegroundColor Green
