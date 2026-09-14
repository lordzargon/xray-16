# OpenXRay Android ARM64 Port Directory

This directory houses Android-specific assets, sysroot staging, and packaging configurations.

## Architecture Guidelines (`.agentrules`)
- **Architecture**: `arm64-v8a`
- **Minimum Platform**: Android API 29 (Android 10)
- **Toolchain**: Android NDK Clang, Ninja, C++17/20
- **Rendering Backend**: Native Vulkan backend (`Layers/xrRenderVK/`) adhering to TBDR architecture principles.

## Directory Layout
- `sysroot/arm64-v8a/`: Staged cross-compiled dependencies (headers, `.so` binaries, and CMake configuration packages for SDL2, OpenAL Soft, etc.).
- `scripts/`: Host and cross-compilation helper scripts.

## Quick Start
From the repository root:
```powershell
# PowerShell (Windows)
.\scripts\build_android.ps1 -Config Release
```
```bash
# Bash (Linux/macOS/CI)
./scripts/build_android.sh --config Release
```
## Building the Android APK
From the `android/` directory:
```powershell
.\gradlew.bat assembleDebug
```
The output APK is generated at:
`android/app/build/outputs/apk/debug/app-debug.apk`

All 20 compiled OpenXRay engine modules, `xr_3da.so`, `libSDL2.so`, `libopenal.so`, and `libc++_shared.so` are automatically staged and packaged into `lib/arm64-v8a/`.

## Deploying Game Assets to Device
To deploy S.T.A.L.K.E.R. Call of Pripyat assets (e.g. from GOG Galaxy), OpenXRay OpenGL shaders, and the Android `fsgame.ltx` to a connected device:
```powershell
# Dry run check
.\scripts\deploy_cop_assets.ps1 -DryRun

# Deploy assets, install APK, and launch game
.\scripts\deploy_cop_assets.ps1 -InstallApk -LaunchApp
```
