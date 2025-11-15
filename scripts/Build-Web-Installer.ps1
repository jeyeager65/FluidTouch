#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Builds FluidTouch firmware for web-based installer.

.DESCRIPTION
    Automates the build process for both hardware variants (Basic and Advance) and prepares
    the firmware files for the ESP Web Tools installer hosted in the web/ directory.
    
    This script:
    1. Builds firmware for both elecrow-crowpanel-7-basic and elecrow-crowpanel-7-advance
    2. Creates web/firmware/basic/ and web/firmware/advance/ directories
    3. Copies all required firmware components (firmware.bin, bootloader.bin, partitions.bin, boot_app0.bin)
    4. Displays build summary with firmware sizes
    5. Provides instructions for local testing
    
    The web installer allows users to flash firmware directly from a browser using ESP Web Tools,
    eliminating the need to install PlatformIO or command-line tools.

.PARAMETER Clean
    Optional. Performs a clean build by removing .pio/build directories first.

.EXAMPLE
    .\build-web-installer.ps1
    Builds both firmware variants and prepares web installer

.EXAMPLE
    .\build-web-installer.ps1 -Clean
    Performs clean build before preparing web installer

.NOTES
    Requirements:
    - PlatformIO installed at $env:USERPROFILE\.platformio\penv\Scripts\platformio.exe
    - Run from FluidTouch project root directory
    - Requires elecrow-crowpanel-7-basic and elecrow-crowpanel-7-advance environments in platformio.ini
    
    Output:
    - web/firmware/basic/*.bin (4MB flash variant)
    - web/firmware/advance/*.bin (16MB flash variant)
    
    Hardware Variants:
    - Basic: ESP32-S3-WROOM-1-N4R8, TN LCD, PWM backlight
    - Advance: ESP32-S3-WROOM-1-N16R8, IPS LCD, I2C backlight
#>

[CmdletBinding()]
param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Determine script location and navigate to project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
Set-Location $projectRoot

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "FluidTouch Web Installer Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Verify we're in the project root
if (-not (Test-Path "platformio.ini")) {
    Write-Host "ERROR: platformio.ini not found" -ForegroundColor Red
    Write-Host "Please run this script from the FluidTouch project root directory" -ForegroundColor Yellow
    exit 1
}

# Verify we're in the project root
if (-not (Test-Path "platformio.ini")) {
    Write-Host "ERROR: platformio.ini not found" -ForegroundColor Red
    Write-Host "Please run this script from the FluidTouch project root directory" -ForegroundColor Yellow
    exit 1
}

# Get PlatformIO path
$platformioExe = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"

if (-not (Test-Path $platformioExe)) {
    Write-Host "ERROR: PlatformIO not found at $platformioExe" -ForegroundColor Red
    Write-Host "Please install PlatformIO first: https://platformio.org/install" -ForegroundColor Yellow
    exit 1
}

# Clean build if requested
if ($Clean) {
    Write-Host "[0/4] Cleaning previous builds..." -ForegroundColor Yellow
    if (Test-Path ".pio/build") {
        Remove-Item -Path ".pio/build" -Recurse -Force
        Write-Host "✓ Build directory cleaned" -ForegroundColor Green
    } else {
        Write-Host "✓ No previous builds to clean" -ForegroundColor Green
    }
    Write-Host ""
}

# Build Basic hardware firmware
Write-Host "[1/4] Building Basic hardware firmware..." -ForegroundColor Yellow
Write-Host "  Environment: elecrow-crowpanel-7-basic" -ForegroundColor Gray
Write-Host "  Hardware: ESP32-S3-WROOM-1-N4R8 (4MB Flash, 8MB PSRAM)" -ForegroundColor Gray
& $platformioExe run --environment elecrow-crowpanel-7-basic

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Basic firmware build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Basic firmware built successfully" -ForegroundColor Green
Write-Host ""

# Build Advance hardware firmware
Write-Host "[2/4] Building Advance hardware firmware..." -ForegroundColor Yellow
Write-Host "  Environment: elecrow-crowpanel-7-advance" -ForegroundColor Gray
Write-Host "  Hardware: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)" -ForegroundColor Gray
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
Write-Host "[4/4] Copying firmware files to web installer..." -ForegroundColor Yellow

# Get boot_app0.bin path (required for OTA updates)
$bootApp0 = "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"

if (-not (Test-Path $bootApp0)) {
    Write-Host "ERROR: boot_app0.bin not found at $bootApp0" -ForegroundColor Red
    Write-Host "This file is required for ESP32 partition initialization" -ForegroundColor Yellow
    exit 1
}

# Copy Basic firmware (4 files required for ESP Web Tools)
Write-Host "  Copying Basic firmware files..." -ForegroundColor Gray
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/firmware.bin" "web/firmware/basic/firmware.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/bootloader.bin" "web/firmware/basic/bootloader.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/partitions.bin" "web/firmware/basic/partitions.bin"
Copy-Item $bootApp0 "web/firmware/basic/boot_app0.bin"

# Copy Advance firmware (4 files required for ESP Web Tools)
Write-Host "  Copying Advance firmware files..." -ForegroundColor Gray
Copy-Item ".pio/build/elecrow-crowpanel-7-advance/firmware.bin" "web/firmware/advance/firmware.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance/bootloader.bin" "web/firmware/advance/bootloader.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance/partitions.bin" "web/firmware/advance/partitions.bin"
Copy-Item $bootApp0 "web/firmware/advance/boot_app0.bin"

Write-Host "✓ All firmware files copied" -ForegroundColor Green
Write-Host ""

# Display file sizes and flash usage
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$basicFirmware = Get-Item "web/firmware/basic/firmware.bin"
$advanceFirmware = Get-Item "web/firmware/advance/firmware.bin"

$basicMB = [math]::Round($basicFirmware.Length / 1MB, 2)
$advanceMB = [math]::Round($advanceFirmware.Length / 1MB, 2)
$basicPercent = [math]::Round(($basicFirmware.Length / 3.25MB) * 100, 0)
$advancePercent = [math]::Round(($advanceFirmware.Length / 6.5MB) * 100, 0)

Write-Host "Basic firmware:   $basicMB MB of 3.25 MB (${basicPercent}% used)" -ForegroundColor White
Write-Host "Advance firmware: $advanceMB MB of 6.5 MB (${advancePercent}% used)" -ForegroundColor White
Write-Host ""
Write-Host "Output directories:" -ForegroundColor Gray
Write-Host "  web/firmware/basic/   - 4 files (bootloader, partitions, boot_app0, firmware)" -ForegroundColor Gray
Write-Host "  web/firmware/advance/ - 4 files (bootloader, partitions, boot_app0, firmware)" -ForegroundColor Gray
Write-Host ""

# Display instructions
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Testing the Web Installer Locally" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Start local HTTP server:" -ForegroundColor White
Write-Host "   cd web" -ForegroundColor Gray
Write-Host "   python -m http.server 8000" -ForegroundColor Gray
Write-Host "   # or use 'npx http-server -p 8000'" -ForegroundColor DarkGray
Write-Host ""
Write-Host "2. Open browser and navigate to:" -ForegroundColor White
Write-Host "   http://localhost:8000" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Connect ESP32-S3 device via USB cable" -ForegroundColor White
Write-Host "   (Chrome/Edge will prompt for device selection)" -ForegroundColor DarkGray
Write-Host ""
Write-Host "4. Select your hardware variant:" -ForegroundColor White
Write-Host "   • Basic - Elecrow CrowPanel 7`" HMI Display (4MB flash)" -ForegroundColor Gray
Write-Host "   • Advance - Elecrow CrowPanel 7`" Advance (16MB flash)" -ForegroundColor Gray
Write-Host ""
Write-Host "5. Click CONNECT and follow on-screen prompts" -ForegroundColor White
Write-Host ""
Write-Host "Note: ESP Web Tools requires HTTPS in production." -ForegroundColor Yellow
Write-Host "      localhost works for testing without SSL." -ForegroundColor Yellow
Write-Host ""
Write-Host "✓ Web installer ready for testing!" -ForegroundColor Green
