# Build (and optionally run) the FluidTouch desktop simulator.
#
#   .\sim\build.ps1            # configure + build
#   .\sim\build.ps1 -Run       # build, then launch
#   .\sim\build.ps1 -Clean     # wipe sim/build first
#
# Extra arguments after -Run are passed to the simulator, e.g.
#   .\sim\build.ps1 -Run -- --data D:\temp\ft-data
#
# Requires MSYS2 (UCRT64) with gcc, SDL2, cmake and ninja - see sim/README.md.

param(
    [switch]$Run,
    [switch]$Clean,
    [switch]$Release,
    [string]$Msys2Root = $(if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }),
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$SimArgs
)

$ErrorActionPreference = "Stop"
$simDir = $PSScriptRoot
$buildDir = Join-Path $simDir "build"
$ucrtBin = Join-Path $Msys2Root "ucrt64\bin"

if (-not (Test-Path (Join-Path $ucrtBin "gcc.exe"))) {
    Write-Error "MSYS2 UCRT64 gcc not found at $ucrtBin. Install MSYS2 and run:`n  pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja"
}

# Put the MSYS2 toolchain first so it wins over any other gcc (e.g. Cygwin)
$env:PATH = "$ucrtBin;$env:PATH"

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

$buildType = if ($Release) { "Release" } else { "Debug" }

if (-not (Test-Path (Join-Path $buildDir "build.ninja"))) {
    & cmake -S $simDir -B $buildDir -G Ninja "-DCMAKE_BUILD_TYPE=$buildType" `
        -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Run) {
    $exe = Join-Path $buildDir "fluidtouch_sim.exe"
    $SimArgs = @($SimArgs | Where-Object { $_ -ne "--" })
    & $exe @SimArgs
    exit $LASTEXITCODE
}
