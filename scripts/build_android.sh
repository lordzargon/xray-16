#!/usr/bin/env bash
#
# OpenXRay Android ARM64 Build Entry-Point (Bash)
# Targets arm64-v8a, API 29, Clang, Ninja
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CONFIG="Release"
CLEAN=0
CONFIGURE_ONLY=0
BUILD_ONLY=0
TARGET=""
CUSTOM_NDK=""

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  -c, --config <Debug|Release>  Build configuration (default: Release)"
    echo "  --ndk <path>                  Path to Android NDK"
    echo "  --clean                       Remove build folder prior to build"
    echo "  --configure-only              Only run CMake configure"
    echo "  --build-only                  Only run CMake build"
    echo "  --target <target>             Build specific target"
    echo "  -h, --help                    Show this help message"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--config)
            CONFIG="$2"
            shift 2
            ;;
        --ndk)
            CUSTOM_NDK="$2"
            shift 2
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --configure-only)
            CONFIGURE_ONLY=1
            shift
            ;;
        --build-only)
            BUILD_ONLY=1
            shift
            ;;
        --target)
            TARGET="$2"
            shift 2
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

echo "=== OpenXRay Android ARM64 Build Entry-Point ==="
echo "Project Root : ${PROJECT_ROOT}"
echo "Config       : ${CONFIG}"
echo "Target ABI   : arm64-v8a"
echo "Target API   : 29 (Android 10)"

# Resolve NDK
if [[ -n "${CUSTOM_NDK}" ]]; then
    export ANDROID_NDK_HOME="${CUSTOM_NDK}"
elif [[ -z "${ANDROID_NDK_HOME:-}" ]]; then
    if [[ -n "${ANDROID_NDK_ROOT:-}" ]]; then
        export ANDROID_NDK_HOME="${ANDROID_NDK_ROOT}"
    elif [[ -n "${ANDROID_NDK:-}" ]]; then
        export ANDROID_NDK_HOME="${ANDROID_NDK}"
    fi
fi

if [[ -z "${ANDROID_NDK_HOME:-}" || ! -f "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" ]]; then
    echo "Error: Android NDK not found or missing android.toolchain.cmake. Set ANDROID_NDK_HOME." >&2
    exit 1
fi

echo "Android NDK  : ${ANDROID_NDK_HOME}"
echo "Toolchain    : ${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake"

PRESET_NAME="android-arm64-$(echo "${CONFIG}" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="${PROJECT_ROOT}/build/${PRESET_NAME}"

if [[ ${CLEAN} -eq 1 && -d "${BUILD_DIR}" ]]; then
    echo "Cleaning build directory: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

cd "${PROJECT_ROOT}"

if [[ ${BUILD_ONLY} -eq 0 ]]; then
    echo -e "\n--- Configuring with preset: ${PRESET_NAME} ---"
    cmake --preset "${PRESET_NAME}"
fi

if [[ ${CONFIGURE_ONLY} -eq 0 ]]; then
    echo -e "\n--- Building preset: ${PRESET_NAME} ---"
    BUILD_ARGS=(--build --preset "${PRESET_NAME}")
    if [[ -n "${TARGET}" ]]; then
        BUILD_ARGS+=(--target "${TARGET}")
    fi
    cmake "${BUILD_ARGS[@]}"
fi

echo -e "\n[SUCCESS] Android build step completed."
