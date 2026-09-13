<#
.SYNOPSIS
    Builds and stages Android ARM64 dependencies (SDL2, OpenAL Soft, LZO, Ogg, Vorbis, Theora)
    into the android/sysroot/arm64-v8a directory.

.DESCRIPTION
    Cross-compiles third-party dependencies required by OpenXRay on Android using the NDK Clang
    toolchain, Ninja, and CMake. Staged output includes headers, static/shared libraries, and CMake
    package files.

.PARAMETER NdkPath
    Custom path to Android NDK. If omitted, checks ANDROID_NDK_HOME, ANDROID_NDK_ROOT, or common installations.

.PARAMETER Deps
    Comma-separated list of dependencies to build: all, sdl2, openal, lzo, ogg, vorbis, theora. Default is "all".

.PARAMETER Force
    Force rebuild even if dependency is already detected in the sysroot.
#>

[CmdletBinding()]
param (
    [string]$NdkPath = "",
    [string]$Deps = "all",
    [switch]$Force,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Full
    exit 0
}

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Sysroot     = Join-Path $ProjectRoot "android\sysroot\arm64-v8a"
$SrcDir      = Join-Path $ProjectRoot "build\deps-src"
$BuildDir    = Join-Path $ProjectRoot "build\deps-build"

# Ensure directories exist
@($Sysroot, $SrcDir, $BuildDir, (Join-Path $Sysroot "include"), (Join-Path $Sysroot "lib")) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -ItemType Directory -Path $_ -Force | Out-Null }
}

# 1. Resolve Android NDK
if (-not $NdkPath) {
    if ($env:ANDROID_NDK_HOME -and (Test-Path $env:ANDROID_NDK_HOME)) {
        $NdkPath = $env:ANDROID_NDK_HOME
    } elseif ($env:ANDROID_NDK_ROOT -and (Test-Path $env:ANDROID_NDK_ROOT)) {
        $NdkPath = $env:ANDROID_NDK_ROOT
    } else {
        $unityNdk = "C:\Program Files\Unity\Hub\Editor\2023.2.2f1\Editor\Data\PlaybackEngines\AndroidPlayer\NDK"
        if (Test-Path $unityNdk) {
            $NdkPath = $unityNdk
        } else {
            $sdkNdkRoot = "$env:LOCALAPPDATA\Android\Sdk\ndk"
            if (Test-Path $sdkNdkRoot) {
                $latest = Get-ChildItem -Path $sdkNdkRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
                if ($latest) { $NdkPath = $latest.FullName }
            }
        }
    }
}

if (-not $NdkPath -or -not (Test-Path $NdkPath)) {
    Write-Error "Android NDK not found. Please provide -NdkPath or set ANDROID_NDK_HOME."
    exit 1
}

$ToolchainFile = Join-Path $NdkPath "build\cmake\android.toolchain.cmake"
if (-not (Test-Path $ToolchainFile)) {
    Write-Error "Toolchain file not found at: $ToolchainFile"
    exit 1
}

# 2. Resolve CMake and Ninja
$CMakeExe = "cmake"
$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCmd) {
    $commonCmake = "C:\Program Files\CMake\bin\cmake.exe"
    if (Test-Path $commonCmake) {
        $CMakeExe = $commonCmake
    } else {
        Write-Error "CMake executable was not found. Please install CMake 3.23+."
        exit 1
    }
}

$ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninjaCmd) {
    $wingetNinja = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages"
    $found = Get-ChildItem -Path $wingetNinja -Filter "ninja.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $env:PATH = "$($found.Directory.FullName);$env:PATH"
    }
}

Write-Host "=== OpenXRay Android Dependency Staging ===" -ForegroundColor Cyan
Write-Host "Project Root : $ProjectRoot"
Write-Host "Sysroot      : $Sysroot"
Write-Host "NDK Path     : $NdkPath"
Write-Host "CMake        : $CMakeExe"
Write-Host "Dependencies : $Deps"
Write-Host ""

$CommonCmakeArgs = @(
    "-G", "Ninja",
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile",
    "-DANDROID_ABI=arm64-v8a",
    "-DANDROID_PLATFORM=android-29",
    "-DANDROID_STL=c++_shared",
    "-DANDROID_ARM_NEON=TRUE",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    "-DCMAKE_INSTALL_PREFIX=$Sysroot",
    "-DCMAKE_PREFIX_PATH=$Sysroot",
    "-DCMAKE_FIND_ROOT_PATH=$Sysroot"
)

function Exec-Step([string]$Desc, [scriptblock]$Action) {
    Write-Host "--> $Desc..." -ForegroundColor Yellow
    & $Action
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        Write-Error "Step failed with exit code $($LASTEXITCODE): $Desc"
        exit $LASTEXITCODE
    }
}

$depsList = $Deps.ToLower().Split(",") | ForEach-Object { $_.Trim() }
$buildAll = ($depsList -contains "all")

# -------------------------------------------------------------
# 1. SDL2
# -------------------------------------------------------------
if ($buildAll -or ($depsList -contains "sdl2")) {
    $sdlInstalled = (Test-Path "$Sysroot\include\SDL2\SDL.h") -and (Test-Path "$Sysroot\lib\libSDL2.so")
    if (-not $sdlInstalled -or $Force) {
        Write-Host "=== Building SDL2 ===" -ForegroundColor Green
        $sdlSrc = Join-Path $SrcDir "SDL2"
        if (-not (Test-Path $sdlSrc)) {
            Exec-Step "Cloning SDL2 (release-2.30.8)" {
                git clone --depth 1 --branch release-2.30.8 https://github.com/libsdl-org/SDL.git $sdlSrc
            }
        }
        $sdlBuild = Join-Path $BuildDir "SDL2"
        if (Test-Path $sdlBuild) { Remove-Item -Path $sdlBuild -Recurse -Force }
        New-Item -ItemType Directory -Path $sdlBuild -Force | Out-Null

        Exec-Step "Configuring SDL2" {
            & $CMakeExe -B $sdlBuild -S $sdlSrc @CommonCmakeArgs `
                -DSDL_SHARED=ON `
                -DSDL_STATIC=OFF `
                -DSDL_TESTS=OFF `
                -DSDL_RENDER=OFF
        }
        Exec-Step "Building & Installing SDL2" {
            & $CMakeExe --build $sdlBuild --target install
        }
    } else {
        Write-Host "SDL2 is already staged. (Use -Force to rebuild)" -ForegroundColor DarkGray
    }
}

# -------------------------------------------------------------
# 2. OpenAL Soft
# -------------------------------------------------------------
if ($buildAll -or ($depsList -contains "openal")) {
    $alInstalled = (Test-Path "$Sysroot\include\AL\al.h") -and (Test-Path "$Sysroot\lib\libopenal.so")
    if (-not $alInstalled -or $Force) {
        Write-Host "=== Building OpenAL Soft ===" -ForegroundColor Green
        $alSrc = Join-Path $SrcDir "openal-soft"
        if (-not (Test-Path $alSrc)) {
            Exec-Step "Cloning OpenAL Soft (1.23.1)" {
                git clone --depth 1 --branch 1.23.1 https://github.com/kcat/openal-soft.git $alSrc
            }
        }
        $alBuild = Join-Path $BuildDir "openal-soft"
        if (Test-Path $alBuild) { Remove-Item -Path $alBuild -Recurse -Force }
        New-Item -ItemType Directory -Path $alBuild -Force | Out-Null

        Exec-Step "Configuring OpenAL Soft" {
            & $CMakeExe -B $alBuild -S $alSrc @CommonCmakeArgs `
                -DBUILD_SHARED_LIBS=ON `
                -DALSOFT_BACKEND_OPENSL=ON `
                -DALSOFT_EMBED_HRTF_DATA=YES `
                -DALSOFT_EXAMPLES=OFF `
                -DALSOFT_TESTS=OFF `
                -DALSOFT_UTILS=OFF
        }
        Exec-Step "Building & Installing OpenAL Soft" {
            & $CMakeExe --build $alBuild --target install
        }
    } else {
        Write-Host "OpenAL Soft is already staged. (Use -Force to rebuild)" -ForegroundColor DarkGray
    }
}

# -------------------------------------------------------------
# 3. LZO 2.10
# -------------------------------------------------------------
if ($buildAll -or ($depsList -contains "lzo")) {
    $lzoInstalled = (Test-Path "$Sysroot\include\lzo\lzo1x.h") -and (
        (Test-Path "$Sysroot\lib\liblzo2.a") -or (Test-Path "$Sysroot\lib\liblzo2.so")
    )
    if (-not $lzoInstalled -or $Force) {
        Write-Host "=== Building LZO 2.10 ===" -ForegroundColor Green
        $lzoSrc = Join-Path $SrcDir "lzo-2.10"
        if (-not (Test-Path $lzoSrc)) {
            $lzoArchive = Join-Path $SrcDir "lzo-2.10.tar.gz"
            Exec-Step "Downloading LZO 2.10" {
                curl.exe -s -L -o $lzoArchive "https://www.oberhumer.com/opensource/lzo/download/lzo-2.10.tar.gz"
            }
            Exec-Step "Extracting LZO 2.10" {
                tar.exe -xzf $lzoArchive -C $SrcDir
            }
        }
        $lzoBuild = Join-Path $BuildDir "lzo"
        if (Test-Path $lzoBuild) { Remove-Item -Path $lzoBuild -Recurse -Force }
        New-Item -ItemType Directory -Path $lzoBuild -Force | Out-Null

        Exec-Step "Configuring LZO" {
            & $CMakeExe -B $lzoBuild -S $lzoSrc @CommonCmakeArgs `
                -DENABLE_STATIC=ON `
                -DENABLE_SHARED=OFF `
                -DENABLE_EXAMPLES=OFF
        }
        Exec-Step "Building & Installing LZO" {
            & $CMakeExe --build $lzoBuild --target install
        }
    } else {
        Write-Host "LZO is already staged. (Use -Force to rebuild)" -ForegroundColor DarkGray
    }
}

# -------------------------------------------------------------
# 4. libogg 1.3.5
# -------------------------------------------------------------
if ($buildAll -or ($depsList -contains "ogg")) {
    $oggInstalled = (Test-Path "$Sysroot\include\ogg\ogg.h") -and (
        (Test-Path "$Sysroot\lib\libogg.a") -or (Test-Path "$Sysroot\lib\libogg.so")
    )
    if (-not $oggInstalled -or $Force) {
        Write-Host "=== Building libogg ===" -ForegroundColor Green
        $oggSrc = Join-Path $SrcDir "ogg"
        if (-not (Test-Path $oggSrc)) {
            Exec-Step "Cloning libogg (v1.3.5)" {
                git clone --depth 1 --branch v1.3.5 https://github.com/xiph/ogg.git $oggSrc
            }
        }
        $oggBuild = Join-Path $BuildDir "ogg"
        if (Test-Path $oggBuild) { Remove-Item -Path $oggBuild -Recurse -Force }
        New-Item -ItemType Directory -Path $oggBuild -Force | Out-Null

        Exec-Step "Configuring libogg" {
            & $CMakeExe -B $oggBuild -S $oggSrc @CommonCmakeArgs `
                -DBUILD_SHARED_LIBS=OFF `
                -DINSTALL_DOCS=OFF
        }
        Exec-Step "Building & Installing libogg" {
            & $CMakeExe --build $oggBuild --target install
        }
    } else {
        Write-Host "libogg is already staged. (Use -Force to rebuild)" -ForegroundColor DarkGray
    }
}

# -------------------------------------------------------------
# 5. libvorbis 1.3.7
# -------------------------------------------------------------
if ($buildAll -or ($depsList -contains "vorbis")) {
    $vorbisInstalled = (Test-Path "$Sysroot\include\vorbis\codec.h") -and (
        (Test-Path "$Sysroot\lib\libvorbis.a") -or (Test-Path "$Sysroot\lib\libvorbis.so")
    ) -and (
        (Test-Path "$Sysroot\lib\libvorbisfile.a") -or (Test-Path "$Sysroot\lib\libvorbisfile.so")
    )
    if (-not $vorbisInstalled -or $Force) {
        Write-Host "=== Building libvorbis ===" -ForegroundColor Green
        $vorbisSrc = Join-Path $SrcDir "vorbis"
        if (-not (Test-Path $vorbisSrc)) {
            Exec-Step "Cloning libvorbis (v1.3.7)" {
                git clone --depth 1 --branch v1.3.7 https://github.com/xiph/vorbis.git $vorbisSrc
            }
        }
        $vorbisBuild = Join-Path $BuildDir "vorbis"
        if (Test-Path $vorbisBuild) { Remove-Item -Path $vorbisBuild -Recurse -Force }
        New-Item -ItemType Directory -Path $vorbisBuild -Force | Out-Null

        Exec-Step "Configuring libvorbis" {
            & $CMakeExe -B $vorbisBuild -S $vorbisSrc @CommonCmakeArgs `
                -DBUILD_SHARED_LIBS=OFF
        }
        Exec-Step "Building & Installing libvorbis" {
            & $CMakeExe --build $vorbisBuild --target install
        }
    } else {
        Write-Host "libvorbis is already staged. (Use -Force to rebuild)" -ForegroundColor DarkGray
    }
}

# -------------------------------------------------------------
# 6. libtheora 1.1.1
# -------------------------------------------------------------
if ($buildAll -or ($depsList -contains "theora")) {
    $theoraInstalled = (Test-Path "$Sysroot\include\theora\theora.h") -and (
        (Test-Path "$Sysroot\lib\libtheora.a") -or (Test-Path "$Sysroot\lib\libtheora.so")
    ) -and (
        (Test-Path "$Sysroot\lib\libtheoradec.a") -or (Test-Path "$Sysroot\lib\libtheoradec.so")
    )
    if (-not $theoraInstalled -or $Force) {
        Write-Host "=== Building libtheora ===" -ForegroundColor Green
        $theoraSrc = Join-Path $SrcDir "theora"
        if (-not (Test-Path $theoraSrc)) {
            $theoraArchive = Join-Path $SrcDir "libtheora-1.1.1.tar.gz"
            Exec-Step "Downloading libtheora 1.1.1" {
                curl.exe -s -L -o $theoraArchive "https://downloads.xiph.org/releases/theora/libtheora-1.1.1.tar.gz"
            }
            Exec-Step "Extracting libtheora 1.1.1" {
                tar.exe -xzf $theoraArchive -C $SrcDir
            }
            # Rename libtheora-1.1.1 directory to theora if needed
            $extracted = Join-Path $SrcDir "libtheora-1.1.1"
            if (Test-Path $extracted) {
                Move-Item -Path $extracted -Destination $theoraSrc -Force
            }
        }

        # Provide standalone CMakeLists.txt for libtheora
        $theoraCmake = @'
cmake_minimum_required(VERSION 3.20)
project(theora LANGUAGES C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Ogg REQUIRED)

file(GLOB THEORA_HEADERS "include/theora/*.h")

set(THEORA_COMMON_SOURCES
    lib/apiwrapper.c
    lib/bitpack.c
    lib/dequant.c
    lib/fragment.c
    lib/idct.c
    lib/info.c
    lib/internal.c
    lib/state.c
    lib/quant.c
)

set(THEORA_DEC_SOURCES
    lib/decapiwrapper.c
    lib/decinfo.c
    lib/decode.c
    lib/huffdec.c
)

set(THEORA_ENC_SOURCES
    lib/analyze.c
    lib/encapiwrapper.c
    lib/encfrag.c
    lib/encinfo.c
    lib/encode.c
    lib/enquant.c
    lib/fdct.c
    lib/huffenc.c
    lib/mathops.c
    lib/mcenc.c
    lib/rate.c
    lib/tokenize.c
)

add_library(theoradec STATIC ${THEORA_COMMON_SOURCES} ${THEORA_DEC_SOURCES})
target_include_directories(theoradec PUBLIC ${PROJECT_SOURCE_DIR}/include ${OGG_INCLUDE_DIRS})
target_link_libraries(theoradec PUBLIC ${OGG_LIBRARIES})

add_library(theoraenc STATIC ${THEORA_COMMON_SOURCES} ${THEORA_ENC_SOURCES})
target_include_directories(theoraenc PUBLIC ${PROJECT_SOURCE_DIR}/include ${OGG_INCLUDE_DIRS})
target_link_libraries(theoraenc PUBLIC ${OGG_LIBRARIES})

add_library(theora STATIC ${THEORA_COMMON_SOURCES} ${THEORA_DEC_SOURCES} ${THEORA_ENC_SOURCES})
target_include_directories(theora PUBLIC ${PROJECT_SOURCE_DIR}/include ${OGG_INCLUDE_DIRS})
target_link_libraries(theora PUBLIC ${OGG_LIBRARIES})

install(FILES ${THEORA_HEADERS} DESTINATION include/theora)
install(TARGETS theora theoradec theoraenc
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
'@
        Set-Content -Path (Join-Path $theoraSrc "CMakeLists.txt") -Value $theoraCmake -Encoding UTF8

        $theoraBuild = Join-Path $BuildDir "theora"
        if (Test-Path $theoraBuild) { Remove-Item -Path $theoraBuild -Recurse -Force }
        New-Item -ItemType Directory -Path $theoraBuild -Force | Out-Null

        Exec-Step "Configuring libtheora" {
            & $CMakeExe -B $theoraBuild -S $theoraSrc @CommonCmakeArgs `
                -DOGG_INCLUDE_DIRS="$Sysroot/include" `
                -DOGG_LIBRARIES="$Sysroot/lib/libogg.a"
        }
        Exec-Step "Building & Installing libtheora" {
            & $CMakeExe --build $theoraBuild --target install
        }
    } else {
        Write-Host "libtheora is already staged. (Use -Force to rebuild)" -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host "=== Dependency Staging Complete ===" -ForegroundColor Cyan
Write-Host "Staged sysroot contents ($Sysroot):"
Get-ChildItem -Path (Join-Path $Sysroot "lib") | Select-Object Name, Length | Format-Table -AutoSize
