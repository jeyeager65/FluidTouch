#!/usr/bin/env pwsh
# FluidTouch Web Installer Build Script
# Builds firmware for both Basic and Advance hardware variants and copies to web installer directory

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "FluidTouch Web Installer Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Get PlatformIO path
$platformioExe = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"

if (-not (Test-Path $platformioExe)) {
    Write-Host "ERROR: PlatformIO not found at $platformioExe" -ForegroundColor Red
    Write-Host "Please install PlatformIO first." -ForegroundColor Red
    exit 1
}

# Build Basic hardware firmware
Write-Host "[1/4] Building Basic hardware firmware..." -ForegroundColor Yellow
& $platformioExe run --environment elecrow-crowpanel-7-basic

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Basic firmware build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Basic firmware built successfully" -ForegroundColor Green
Write-Host ""

# Build Advance hardware firmware
Write-Host "[2/4] Building Advance hardware firmware..." -ForegroundColor Yellow
& $platformioExe run --environment elecrow-crowpanel-7-advance

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Advance firmware build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Advance firmware built successfully" -ForegroundColor Green
Write-Host ""

# Create web firmware directories
Write-Host "[3/4] Creating web installer directories..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path "web/firmware/basic" -Force | Out-Null
New-Item -ItemType Directory -Path "web/firmware/advance" -Force | Out-Null

Write-Host "✓ Directories created" -ForegroundColor Green
Write-Host ""

# Copy firmware files
Write-Host "[4/4] Copying firmware files..." -ForegroundColor Yellow

# Get boot_app0.bin path
$bootApp0 = "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"

if (-not (Test-Path $bootApp0)) {
    Write-Host "ERROR: boot_app0.bin not found at $bootApp0" -ForegroundColor Red
    exit 1
}

# Copy Basic firmware
Write-Host "  Copying Basic firmware files..." -ForegroundColor Gray
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/firmware.bin" "web/firmware/basic/firmware.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/bootloader.bin" "web/firmware/basic/bootloader.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/partitions.bin" "web/firmware/basic/partitions.bin"
Copy-Item $bootApp0 "web/firmware/basic/boot_app0.bin"

# Copy Advance firmware
Write-Host "  Copying Advance firmware files..." -ForegroundColor Gray
Copy-Item ".pio/build/elecrow-crowpanel-7-advance/firmware.bin" "web/firmware/advance/firmware.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance/bootloader.bin" "web/firmware/advance/bootloader.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance/partitions.bin" "web/firmware/advance/partitions.bin"
Copy-Item $bootApp0 "web/firmware/advance/boot_app0.bin"

Write-Host "✓ All firmware files copied" -ForegroundColor Green
Write-Host ""

# Display file sizes
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$basicFirmware = Get-Item "web/firmware/basic/firmware.bin"
$advanceFirmware = Get-Item "web/firmware/advance/firmware.bin"

Write-Host "Basic firmware:   $([math]::Round($basicFirmware.Length / 1MB, 2)) MB" -ForegroundColor White
Write-Host "Advance firmware: $([math]::Round($advanceFirmware.Length / 1MB, 2)) MB" -ForegroundColor White
Write-Host ""

# Display instructions
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Testing the Web Installer Locally" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "1. Start HTTP server:" -ForegroundColor White
Write-Host "   cd web" -ForegroundColor Gray
Write-Host "   python -m http.server 8000" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Open browser:" -ForegroundColor White
Write-Host "   http://localhost:8000" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Connect ESP32-S3 device via USB" -ForegroundColor White
Write-Host ""
Write-Host "4. Select hardware variant and click CONNECT" -ForegroundColor White
Write-Host ""
Write-Host "✓ Web installer ready for testing!" -ForegroundColor Green
