#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Builds FluidTouch firmware for web-based installer with version management.

.DESCRIPTION
    Automates the build process for both hardware variants (Basic and Advance) and prepares
    the firmware files for the ESP Web Tools installer hosted in the web/ directory.
    
    This script:
    1. Builds firmware for both elecrow-crowpanel-7-basic and elecrow-crowpanel-7-advance
    2. Creates versioned firmware directories (web/firmware/{version}/ or web/firmware/preview/{version}/)
    3. Copies all required firmware components (firmware.bin, bootloader.bin, partitions.bin, boot_app0.bin)
    4. Optionally updates versions.json with new version entry
    5. Displays build summary with firmware sizes
    6. Provides instructions for local testing
    
    The web installer allows users to flash firmware directly from a browser using ESP Web Tools,
    with version selection via dropdown.

.PARAMETER Clean
    Optional. Performs a clean build by removing .pio/build directories first.

.PARAMETER Version
    Optional. Version identifier for the build (default: "local").
    Examples: "1.0.2", "1.0.3", "local"

.PARAMETER UpdateVersions
    Optional. Updates web/versions.json with the new version entry.
    Creates versions.json if it doesn't exist.

.PARAMETER Preview
    Optional. Marks the build as a preview and appends branch name to version.
    Example: version "1.0.2" on branch "feature/status-tab" becomes "1.0.2_status-tab"

.PARAMETER StartServer
    Optional. Skips build and just starts a local HTTP server for testing.
    Equivalent to: cd web && python -m http.server 8000

.EXAMPLE
    .\build-web-installer.ps1
    Builds firmware to web/firmware/basic/ and web/firmware/advance/ (legacy local build)

.EXAMPLE
    .\build-web-installer.ps1 -Clean
    Performs clean build before preparing web installer

.EXAMPLE
    .\build-web-installer.ps1 -Version "1.0.2" -UpdateVersions
    Builds version 1.0.2 to web/firmware/1.0.2/ and updates versions.json

.EXAMPLE
    .\build-web-installer.ps1 -Preview -UpdateVersions
    Builds preview version with branch name (e.g., "1.0.2_dev") and updates versions.json

.EXAMPLE
    .\build-web-installer.ps1 -StartServer
    Starts local HTTP server at http://localhost:8000 for testing

.NOTES
    Requirements:
    - PlatformIO installed at $env:USERPROFILE\.platformio\penv\Scripts\platformio.exe
    - Run from FluidTouch project root directory
    - Requires elecrow-crowpanel-7-basic and elecrow-crowpanel-7-advance environments in platformio.ini
    
    Output:
    - web/firmware/basic/*.bin (4MB flash variant)
    - web/firmware/advance-v1.3/*.bin (16MB flash, v1.3 backlight)
    - web/firmware/advance-v1.2/*.bin (16MB flash, v1.2 backlight)
    
    Hardware Variants:
    - Basic: ESP32-S3-WROOM-1-N4R8, TN LCD, PWM backlight
    - Advance v1.3: ESP32-S3-WROOM-1-N16R8, IPS LCD, I2C backlight (continuous 0-245)
    - Advance v1.2: ESP32-S3-WROOM-1-N16R8, IPS LCD, I2C backlight (discrete 0x05-0x10)
#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [string]$Version = "local",
    [switch]$UpdateVersions,
    [switch]$Preview,
    [switch]$StartServer
)

$ErrorActionPreference = "Stop"

# Determine script location and navigate to project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
Set-Location $projectRoot

# If StartServer is specified, just start the local server
if ($StartServer) {
    Write-Host "Starting local HTTP server..." -ForegroundColor Yellow
    Write-Host "Navigate to: http://localhost:8000" -ForegroundColor Cyan
    Write-Host "Press Ctrl+C to stop the server" -ForegroundColor Gray
    Write-Host ""
    
    if (Get-Command python -ErrorAction SilentlyContinue) {
        Set-Location web
        python -m http.server 8000
    } else {
        Write-Host "ERROR: Python not found in PATH" -ForegroundColor Red
        Write-Host "Install Python or use: npx http-server -p 8000" -ForegroundColor Yellow
        exit 1
    }
    exit 0
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "FluidTouch Web Installer Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Extract version from manifest if "local" specified and UpdateVersions requested
if ($Version -eq "local" -and $UpdateVersions) {
    Write-Host "Reading version from web/manifest_basic.json..." -ForegroundColor Gray
    $manifestContent = Get-Content "web/manifest_basic.json" -Raw | ConvertFrom-Json
    $Version = $manifestContent.version
    Write-Host "  Version: $Version" -ForegroundColor Gray
}

# Append preview suffix if requested
if ($Preview) {
    # Extract branch name suffix (everything after last /)
    $gitBranch = git rev-parse --abbrev-ref HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $gitBranch) {
        $branchSuffix = $gitBranch.Split('/')[-1]
        $Version = "${Version}_${branchSuffix}"
        Write-Host "Preview mode: Version set to $Version" -ForegroundColor Magenta
    } else {
        Write-Host "WARNING: Preview mode requires git repository" -ForegroundColor Yellow
        Write-Host "         Continuing with version: $Version" -ForegroundColor Yellow
    }
}

Write-Host "Build configuration:" -ForegroundColor Gray
Write-Host "  Version: $Version" -ForegroundColor White
Write-Host "  Update versions.json: $UpdateVersions" -ForegroundColor White
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
Write-Host "[1/5] Building Basic hardware firmware..." -ForegroundColor Yellow
Write-Host "  Environment: elecrow-crowpanel-7-basic" -ForegroundColor Gray
Write-Host "  Hardware: ESP32-S3-WROOM-1-N4R8 (4MB Flash, 8MB PSRAM)" -ForegroundColor Gray
& $platformioExe run --environment elecrow-crowpanel-7-basic

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Basic firmware build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Basic firmware built successfully" -ForegroundColor Green
Write-Host ""

# Build Advance v1.3 hardware firmware
Write-Host "[2/5] Building Advance v1.3 hardware firmware..." -ForegroundColor Yellow
Write-Host "  Environment: elecrow-crowpanel-7-advance-v13" -ForegroundColor Gray
Write-Host "  Hardware: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM, v1.3 backlight)" -ForegroundColor Gray
& $platformioExe run --environment elecrow-crowpanel-7-advance-v13

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Advance v1.3 firmware build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Advance v1.3 firmware built successfully" -ForegroundColor Green
Write-Host ""

# Build Advance v1.2 hardware firmware
Write-Host "[3/5] Building Advance v1.2 hardware firmware..." -ForegroundColor Yellow
Write-Host "  Environment: elecrow-crowpanel-7-advance-v12" -ForegroundColor Gray
Write-Host "  Hardware: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM, v1.2 backlight)" -ForegroundColor Gray
& $platformioExe run --environment elecrow-crowpanel-7-advance-v12

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Advance v1.2 firmware build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Advance v1.2 firmware built successfully" -ForegroundColor Green
Write-Host ""

# Create web firmware directories
Write-Host "[4/5] Creating web installer directories..." -ForegroundColor Yellow

# Determine output path based on version and preview mode
if ($Preview -or $Version -match "_") {
    # Preview builds go to preview subfolder
    $outputBasePath = "web/firmware/preview/$Version"
    Write-Host "  Creating preview directories at: $outputBasePath" -ForegroundColor Gray
} elseif ($Version -eq "local") {
    # Local builds go to root firmware folder (legacy path)
    $outputBasePath = "web/firmware"
    Write-Host "  Creating local directories at: $outputBasePath" -ForegroundColor Gray
} else {
    # Stable releases go to versioned folders
    $outputBasePath = "web/firmware/$Version"
    Write-Host "  Creating version directories at: $outputBasePath" -ForegroundColor Gray
}

New-Item -ItemType Directory -Path "$outputBasePath/basic" -Force | Out-Null
New-Item -ItemType Directory -Path "$outputBasePath/advance-v1.3" -Force | Out-Null
New-Item -ItemType Directory -Path "$outputBasePath/advance-v1.2" -Force | Out-Null

Write-Host "✓ Directories created" -ForegroundColor Green
Write-Host ""

# Copy firmware files
Write-Host "[5/5] Copying firmware files to web installer..." -ForegroundColor Yellow

# Get boot_app0.bin path (required for OTA updates)
$bootApp0 = "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"

if (-not (Test-Path $bootApp0)) {
    Write-Host "ERROR: boot_app0.bin not found at $bootApp0" -ForegroundColor Red
    Write-Host "This file is required for ESP32 partition initialization" -ForegroundColor Yellow
    exit 1
}

# Copy Basic firmware (4 files required for ESP Web Tools)
Write-Host "  Copying Basic firmware files..." -ForegroundColor Gray
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/firmware.bin" "$outputBasePath/basic/firmware.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/bootloader.bin" "$outputBasePath/basic/bootloader.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-basic/partitions.bin" "$outputBasePath/basic/partitions.bin"
Copy-Item $bootApp0 "$outputBasePath/basic/boot_app0.bin"

# Copy Advance v1.3 firmware (4 files required for ESP Web Tools)
Write-Host "  Copying Advance v1.3 firmware files..." -ForegroundColor Gray
Copy-Item ".pio/build/elecrow-crowpanel-7-advance-v13/firmware.bin" "$outputBasePath/advance-v1.3/firmware.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance-v13/bootloader.bin" "$outputBasePath/advance-v1.3/bootloader.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance-v13/partitions.bin" "$outputBasePath/advance-v1.3/partitions.bin"
Copy-Item $bootApp0 "$outputBasePath/advance-v1.3/boot_app0.bin"

# Copy Advance v1.2 firmware (4 files required for ESP Web Tools)
Write-Host "  Copying Advance v1.2 firmware files..." -ForegroundColor Gray
Copy-Item ".pio/build/elecrow-crowpanel-7-advance-v12/firmware.bin" "$outputBasePath/advance-v1.2/firmware.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance-v12/bootloader.bin" "$outputBasePath/advance-v1.2/bootloader.bin"
Copy-Item ".pio/build/elecrow-crowpanel-7-advance-v12/partitions.bin" "$outputBasePath/advance-v1.2/partitions.bin"
Copy-Item $bootApp0 "$outputBasePath/advance-v1.2/boot_app0.bin"

Write-Host "✓ All firmware files copied" -ForegroundColor Green
Write-Host ""

# Copy and update manifest files for versioned builds
if ($Version -ne "local") {
    Write-Host "Copying and updating manifest files..." -ForegroundColor Gray
    
    # Copy manifests to version folder
    Copy-Item "web/manifest_basic.json" "$outputBasePath/manifest_basic.json"
    Copy-Item "web/manifest_advance-v1.3.json" "$outputBasePath/manifest_advance-v1.3.json"
    Copy-Item "web/manifest_advance-v1.2.json" "$outputBasePath/manifest_advance-v1.2.json"
    
    # Update paths in manifests to be relative to manifest location
    $manifestBasic = Get-Content "$outputBasePath/manifest_basic.json" -Raw
    $manifestBasic = $manifestBasic -replace 'firmware/basic/', 'basic/'
    $manifestBasic = $manifestBasic -replace '"version":\s*"[^"]*"', """version"": ""$Version"""
    $manifestBasic | Set-Content "$outputBasePath/manifest_basic.json" -Encoding UTF8
    
    $manifestAdvV13 = Get-Content "$outputBasePath/manifest_advance-v1.3.json" -Raw
    $manifestAdvV13 = $manifestAdvV13 -replace 'firmware/advance-v1.3/', 'advance-v1.3/'
    $manifestAdvV13 = $manifestAdvV13 -replace '"version":\s*"[^"]*"', """version"": ""$Version"""
    $manifestAdvV13 | Set-Content "$outputBasePath/manifest_advance-v1.3.json" -Encoding UTF8
    
    $manifestAdvV12 = Get-Content "$outputBasePath/manifest_advance-v1.2.json" -Raw
    $manifestAdvV12 = $manifestAdvV12 -replace 'firmware/advance-v1.2/', 'advance-v1.2/'
    $manifestAdvV12 = $manifestAdvV12 -replace '"version":\s*"[^"]*"', """version"": ""$Version"""
    $manifestAdvV12 | Set-Content "$outputBasePath/manifest_advance-v1.2.json" -Encoding UTF8
    
    Write-Host "✓ Manifests copied and updated" -ForegroundColor Green
    Write-Host ""
}

# Update versions.json if requested
if ($UpdateVersions) {
    Write-Host "[6/6] Updating versions.json..." -ForegroundColor Yellow
    
    $versionsPath = "web/versions.json"
    
    # Load existing versions.json or create new structure
    if (Test-Path $versionsPath) {
        $versionsData = Get-Content $versionsPath -Raw | ConvertFrom-Json
    } else {
        $versionsData = @{
            default_version = $null
            default_hardware = "advance-v1.3"
            versions = @()
            hardware_info = @{
                basic = "Basic (4MB flash, TN LCD, PWM backlight)"
                "advance-v1.3" = "Advance v1.3 (16MB flash, IPS LCD, continuous brightness)"
                "advance-v1.2" = "Advance v1.2 (16MB flash, IPS LCD, discrete brightness)"
            }
        }
    }
    
    # Create version entry
    $isPreview = $Preview -or $Version -match "_"
    $versionEntry = @{
        id = $Version
        hardware = @("basic", "advance-v1.3", "advance-v1.2")
        is_preview = $isPreview
    }
    
    # Add display name for preview builds
    if ($isPreview) {
        $buildDate = Get-Date -Format "yyyy-MM-dd"
        $versionEntry.display_name = "$Version (Preview $buildDate)"
    }
    
    # Remove existing entry with same ID if present
    $versionsData.versions = @($versionsData.versions | Where-Object { $_.id -ne $Version })
    
    # Prepend new version to array
    $versionsData.versions = @($versionEntry) + $versionsData.versions
    
    # Set default version if this is a stable release
    if (-not $isPreview) {
        $versionsData.default_version = $Version
    }
    
    # Save versions.json with pretty formatting
    $versionsData | ConvertTo-Json -Depth 10 | Set-Content $versionsPath -Encoding UTF8
    
    Write-Host "✓ versions.json updated with version: $Version" -ForegroundColor Green
    if ($isPreview) {
        Write-Host "  (marked as preview)" -ForegroundColor Magenta
    }
    Write-Host ""
}

# Display file sizes and flash usage
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$basicFirmware = Get-Item "$outputBasePath/basic/firmware.bin"
$advanceV13Firmware = Get-Item "$outputBasePath/advance-v1.3/firmware.bin"
$advanceV12Firmware = Get-Item "$outputBasePath/advance-v1.2/firmware.bin"

$basicMB = [math]::Round($basicFirmware.Length / 1MB, 2)
$advanceV13MB = [math]::Round($advanceV13Firmware.Length / 1MB, 2)
$advanceV12MB = [math]::Round($advanceV12Firmware.Length / 1MB, 2)
$basicPercent = [math]::Round(($basicFirmware.Length / 3.25MB) * 100, 0)
$advanceV13Percent = [math]::Round(($advanceV13Firmware.Length / 6.5MB) * 100, 0)
$advanceV12Percent = [math]::Round(($advanceV12Firmware.Length / 6.5MB) * 100, 0)

Write-Host "Version: $Version" -ForegroundColor White
if ($Preview -or $Version -match "_") {
    Write-Host "  (Preview Build)" -ForegroundColor Magenta
}
Write-Host ""
Write-Host "Basic firmware:       $basicMB MB of 3.25 MB (${basicPercent}% used)" -ForegroundColor White
Write-Host "Advance v1.3 firmware: $advanceV13MB MB of 6.5 MB (${advanceV13Percent}% used)" -ForegroundColor White
Write-Host "Advance v1.2 firmware: $advanceV12MB MB of 6.5 MB (${advanceV12Percent}% used)" -ForegroundColor White
Write-Host ""
Write-Host "Output directories:" -ForegroundColor Gray
Write-Host "  $outputBasePath/basic/        - 4 files (bootloader, partitions, boot_app0, firmware)" -ForegroundColor Gray
Write-Host "  $outputBasePath/advance-v1.3/ - 4 files (bootloader, partitions, boot_app0, firmware)" -ForegroundColor Gray
Write-Host "  $outputBasePath/advance-v1.2/ - 4 files (bootloader, partitions, boot_app0, firmware)" -ForegroundColor Gray
Write-Host ""

# Display instructions
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Testing the Web Installer Locally" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Quick Start:" -ForegroundColor White
Write-Host "  .\scripts\build-web-installer.ps1 -StartServer" -ForegroundColor Cyan
Write-Host ""
Write-Host "Or manually:" -ForegroundColor White
Write-Host ""
Write-Host "1. Start local HTTP server:" -ForegroundColor White
Write-Host "   cd web" -ForegroundColor Gray
Write-Host "   python -m http.server 8000" -ForegroundColor Gray
Write-Host "   # or use 'npx http-server -p 8000'" -ForegroundColor DarkGray
Write-Host ""
Write-Host "2. Open browser and navigate to:" -ForegroundColor White
Write-Host "   http://localhost:8000" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Select version and hardware:" -ForegroundColor White
if ($UpdateVersions) {
    Write-Host "   • Version dropdown will show: $Version" -ForegroundColor Gray
} else {
    Write-Host "   • Version dropdown will show available versions" -ForegroundColor Gray
}
Write-Host "   • Hardware: Basic (4MB), Advance v1.3 (16MB), or Advance v1.2 (16MB)" -ForegroundColor Gray
Write-Host ""
Write-Host "4. Connect ESP32-S3 device via USB cable" -ForegroundColor White
Write-Host "   (Chrome/Edge will prompt for device selection)" -ForegroundColor DarkGray
Write-Host ""
Write-Host "5. Click CONNECT and follow on-screen prompts" -ForegroundColor White
Write-Host ""

if ($UpdateVersions) {
    Write-Host "Manifest paths created:" -ForegroundColor Cyan
    if ($Preview -or $Version -match "_") {
        Write-Host "  firmware/preview/$Version/manifest_basic.json" -ForegroundColor Gray
        Write-Host "  firmware/preview/$Version/manifest_advance-v1.3.json" -ForegroundColor Gray
        Write-Host "  firmware/preview/$Version/manifest_advance-v1.2.json" -ForegroundColor Gray
    } elseif ($Version -ne "local") {
        Write-Host "  firmware/$Version/manifest_basic.json" -ForegroundColor Gray
        Write-Host "  firmware/$Version/manifest_advance-v1.3.json" -ForegroundColor Gray
        Write-Host "  firmware/$Version/manifest_advance-v1.2.json" -ForegroundColor Gray
    }
    Write-Host ""
}

Write-Host "Note: ESP Web Tools requires HTTPS in production." -ForegroundColor Yellow
Write-Host "      localhost works for testing without SSL." -ForegroundColor Yellow
Write-Host ""
Write-Host "✓ Web installer ready for testing!" -ForegroundColor Green
