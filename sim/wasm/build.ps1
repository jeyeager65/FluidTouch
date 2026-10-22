# Build FluidTouch for a web page (Emscripten): sim/wasm/build/fluidtouch.js + .wasm
#
#   .\sim\wasm\build.ps1                  # configure + build (Release)
#   .\sim\wasm\build.ps1 -Clean           # wipe sim/wasm/build first
#   .\sim\wasm\build.ps1 -Emsdk D:\emsdk  # emsdk location (default: $env:EMSDK, else C:\emsdk)
#
# Requires emsdk (activated "latest") and MSYS2's cmake + ninja (the same
# packages the desktop simulator uses - see sim/README.md).

param(
    [switch]$Clean,
    [string]$Emsdk = $(if ($env:EMSDK) { $env:EMSDK } else { "C:\emsdk" }),
    [string]$Msys2Root = $(if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" })
)

$ErrorActionPreference = "Stop"
$simDir = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $PSScriptRoot "build"
$ucrtBin = Join-Path $Msys2Root "ucrt64\bin"
$toolchain = Join-Path $Emsdk "upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake"

if (-not (Test-Path $toolchain)) { Write-Error "Emscripten not found under $Emsdk (install emsdk, or pass -Emsdk)" }
if (-not (Test-Path (Join-Path $ucrtBin "cmake.exe"))) { Write-Error "MSYS2 cmake not found at $ucrtBin" }

# emsdk's environment (emcc, node, python) without changing the user's settings
& (Join-Path $Emsdk "emsdk_env.ps1") | Out-Null
$env:PATH = "$env:PATH;$ucrtBin"

if ($Clean -and (Test-Path $buildDir)) { Remove-Item -Recurse -Force $buildDir }

if (-not (Test-Path (Join-Path $buildDir "build.ninja"))) {
    & cmake -S $simDir -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& cmake --build $buildDir
exit $LASTEXITCODE
