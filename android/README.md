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
Or directly with CMake 3.23+:
```bash
cmake --preset android-arm64-release
cmake --build --preset android-arm64-release
```
