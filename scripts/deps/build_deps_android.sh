#!/usr/bin/env bash
# OpenXRay Android ARM64 Dependency Staging Script (Linux/macOS/CI)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SYSROOT="${PROJECT_ROOT}/android/sysroot/arm64-v8a"
SRC_DIR="${PROJECT_ROOT}/build/deps-src"
BUILD_DIR="${PROJECT_ROOT}/build/deps-build"

NDK_PATH="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
DEPS="all"
FORCE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ndk)
      NDK_PATH="$2"
      shift 2
      ;;
    --deps)
      DEPS="$2"
      shift 2
      ;;
    --force)
      FORCE=1
      shift
      ;;
    --help|-h)
      echo "Usage: $0 [--ndk <path>] [--deps <all|sdl2|openal|lzo|ogg|vorbis|theora>] [--force]"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

if [[ -z "${NDK_PATH}" || ! -d "${NDK_PATH}" ]]; then
  echo "Error: Android NDK not found. Set ANDROID_NDK_HOME or pass --ndk <path>." >&2
  exit 1
fi

TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/android.toolchain.cmake"
if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
  echo "Error: Toolchain file not found at ${TOOLCHAIN_FILE}" >&2
  exit 1
fi

mkdir -p "${SYSROOT}/include" "${SYSROOT}/lib" "${SRC_DIR}" "${BUILD_DIR}"

COMMON_CMAKE_ARGS=(
  "-G" "Ninja"
  "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}"
  "-DANDROID_ABI=arm64-v8a"
  "-DANDROID_PLATFORM=android-29"
  "-DANDROID_STL=c++_shared"
  "-DANDROID_ARM_NEON=TRUE"
  "-DCMAKE_BUILD_TYPE=Release"
  "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
  "-DCMAKE_INSTALL_PREFIX=${SYSROOT}"
  "-DCMAKE_PREFIX_PATH=${SYSROOT}"
  "-DCMAKE_FIND_ROOT_PATH=${SYSROOT}"
)

# 1. SDL2
if [[ "${DEPS}" == "all" || "${DEPS}" =~ "sdl2" ]]; then
  if [[ ! -f "${SYSROOT}/lib/libSDL2.so" || ${FORCE} -eq 1 ]]; then
    echo "=== Building SDL2 ==="
    if [[ ! -d "${SRC_DIR}/SDL2" ]]; then
      git clone --depth 1 --branch release-2.30.8 https://github.com/libsdl-org/SDL.git "${SRC_DIR}/SDL2"
    fi
    rm -rf "${BUILD_DIR}/SDL2"
    cmake -B "${BUILD_DIR}/SDL2" -S "${SRC_DIR}/SDL2" "${COMMON_CMAKE_ARGS[@]}" \
      -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_RENDER=OFF
    cmake --build "${BUILD_DIR}/SDL2" --target install
  fi
fi

# 2. OpenAL Soft
if [[ "${DEPS}" == "all" || "${DEPS}" =~ "openal" ]]; then
  if [[ ! -f "${SYSROOT}/lib/libopenal.so" || ${FORCE} -eq 1 ]]; then
    echo "=== Building OpenAL Soft ==="
    if [[ ! -d "${SRC_DIR}/openal-soft" ]]; then
      git clone --depth 1 --branch 1.23.1 https://github.com/kcat/openal-soft.git "${SRC_DIR}/openal-soft"
    fi
    rm -rf "${BUILD_DIR}/openal-soft"
    cmake -B "${BUILD_DIR}/openal-soft" -S "${SRC_DIR}/openal-soft" "${COMMON_CMAKE_ARGS[@]}" \
      -DBUILD_SHARED_LIBS=ON -DALSOFT_BACKEND_OPENSL=ON -DALSOFT_EMBED_HRTF_DATA=YES \
      -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF -DALSOFT_UTILS=OFF
    cmake --build "${BUILD_DIR}/openal-soft" --target install
  fi
fi

# 3. LZO
if [[ "${DEPS}" == "all" || "${DEPS}" =~ "lzo" ]]; then
  if [[ ! -f "${SYSROOT}/lib/liblzo2.a" || ${FORCE} -eq 1 ]]; then
    echo "=== Building LZO ==="
    if [[ ! -d "${SRC_DIR}/lzo-2.10" ]]; then
      curl -s -L -o "${SRC_DIR}/lzo-2.10.tar.gz" "https://www.oberhumer.com/opensource/lzo/download/lzo-2.10.tar.gz"
      tar -xzf "${SRC_DIR}/lzo-2.10.tar.gz" -C "${SRC_DIR}"
    fi
    rm -rf "${BUILD_DIR}/lzo"
    cmake -B "${BUILD_DIR}/lzo" -S "${SRC_DIR}/lzo-2.10" "${COMMON_CMAKE_ARGS[@]}" \
      -DENABLE_STATIC=ON -DENABLE_SHARED=OFF -DENABLE_EXAMPLES=OFF
    cmake --build "${BUILD_DIR}/lzo" --target install
  fi
fi

# 4. libogg
if [[ "${DEPS}" == "all" || "${DEPS}" =~ "ogg" ]]; then
  if [[ ! -f "${SYSROOT}/lib/libogg.a" || ${FORCE} -eq 1 ]]; then
    echo "=== Building libogg ==="
    if [[ ! -d "${SRC_DIR}/ogg" ]]; then
      git clone --depth 1 --branch v1.3.5 https://github.com/xiph/ogg.git "${SRC_DIR}/ogg"
    fi
    rm -rf "${BUILD_DIR}/ogg"
    cmake -B "${BUILD_DIR}/ogg" -S "${SRC_DIR}/ogg" "${COMMON_CMAKE_ARGS[@]}" \
      -DBUILD_SHARED_LIBS=OFF -DINSTALL_DOCS=OFF
    cmake --build "${BUILD_DIR}/ogg" --target install
  fi
fi

# 5. libvorbis
if [[ "${DEPS}" == "all" || "${DEPS}" =~ "vorbis" ]]; then
  if [[ ! -f "${SYSROOT}/lib/libvorbis.a" || ${FORCE} -eq 1 ]]; then
    echo "=== Building libvorbis ==="
    if [[ ! -d "${SRC_DIR}/vorbis" ]]; then
      git clone --depth 1 --branch v1.3.7 https://github.com/xiph/vorbis.git "${SRC_DIR}/vorbis"
    fi
    rm -rf "${BUILD_DIR}/vorbis"
    cmake -B "${BUILD_DIR}/vorbis" -S "${SRC_DIR}/vorbis" "${COMMON_CMAKE_ARGS[@]}" \
      -DBUILD_SHARED_LIBS=OFF
    cmake --build "${BUILD_DIR}/vorbis" --target install
  fi
fi

echo "=== Dependency Staging Complete ==="
ls -la "${SYSROOT}/lib"
