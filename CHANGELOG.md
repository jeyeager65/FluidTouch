# Changelog

All notable changes to FluidTouch will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.3] - 2026-02-10

### Added

- **Advance Hardware v1.2 Support** - Added support for Elecrow CrowPanel 7" Advance v1.2 displays (⚠️ untested) (#11)
  - Note: v1.2 requires same DIP switch configuration as v1.3 (S0 and S1 both set to position 1)
- **mDNS Hostname Resolution** - Connect to FluidNC by hostname (e.g., `fluidnc.local`) instead of IP address (#26)
- **Upload Directory Auto-Creation** - Automatically creates upload directory if it doesn't exist when uploading files from Display SD
- **Open Upload Folder Button** - Quick access button to open the uploaded files folder on FluidNC
- **WCS Display Enhancements** - Improved Work Coordinate System labeling and lock status indicators

### Fixed

- **Display Flicker** - Eliminated screen flicker by migrating to ESP-IDF 5.3 and optimizing display timing parameters
- **Basic Hardware Touch Response** - Improved touch sensitivity and reliability on Basic hardware variant (#21, #23)
- **Probe After WCS Confirmation** - Fixed probe operations failing after confirming Work Coordinate System updates
- **Web Installer Caching** - Prevented browser caching of versions.json to ensure latest version list is always displayed

## [1.0.2] - 2026-01-24

### Changed

- **Status Tab Updates**
  - **Pause/Resume and Stop Buttons** - Pause/Resume/Stop from status tab (#13)
  - **Position Update on Status Tab** - Click positions on status tab to update (#14)
  - **WCS Update on Status Tab** - Click work position values to update Work Coordinate System (#15)
- **Settings Updates**
  - **Display Rotation Support** - Can now rotate display 180 degrees via Settings → General (#12)
- **Web Installer Version Selection** - Dropdown to select from multiple firmware versions including preview builds

### Fixed

- **File Upload with Spaces** - Fixed handling of filenames containing spaces in upload operations

## [1.0.1] - 2025-12-04

### Fixed

- **Touch Screen Deep Sleep Bug** - Fixed touch screen not working after deep sleep wake on Basic hardware
- **Boot Display Flash** - Fixed garbled screen flash on startup

### Changed

- **Documentation Updates** - Product links now use affiliate codes to help support development
- **Hardware Recommendation** - Advance display model now marked as recommended (superior IPS display, optional battery case)

## [1.0.0] - 2025-11-17

### Initial Release

FluidTouch 1.0.0 is the first stable release of the ESP32-S3 touchscreen CNC controller for FluidNC-based machines.

#### Features

- **Real-time Machine Control** - Monitor position, state, feed/spindle rates with live updates
- **Multi-Machine Support** - Store and switch between up to 4 different CNC configurations
- **Intuitive Jogging** - Button-based and analog joystick interfaces with configurable step sizes
- **Touch Probe Operations** - Automated probing with customizable parameters
- **Macro Support** - Configure and store up to 9 file-based macros per machine
- **File Management** - Browse and manage files from FluidNC SD, FluidNC Flash, and Display SD card
- **Settings Backup & Restore** - Export settings to JSON, auto-import on fresh install
- **Power Management** - Configurable display dimming, sleep, and deep sleep modes
- **WiFi Connectivity** - WebSocket connection to FluidNC with automatic status reporting
- **Terminal** - Execute custom commands and view FluidNC messages

#### Supported Hardware

- Elecrow CrowPanel 7" Basic ESP32-S3 HMI Display (4MB Flash + 8MB PSRAM)
- Elecrow CrowPanel 7" Advance ESP32-S3 HMI Display (16MB Flash + 8MB PSRAM)
  - Hardware Version 1.3 only

#### Documentation

- Complete user interface guide with screenshots
- Usage instructions and workflows
- Configuration guide for WiFi, machines, and settings
- Development guide for building from source

[1.0.3]: https://github.com/jeyeager65/FluidTouch/releases/tag/v1.0.3
[1.0.2]: https://github.com/jeyeager65/FluidTouch/releases/tag/v1.0.2
[1.0.1]: https://github.com/jeyeager65/FluidTouch/releases/tag/v1.0.1
[1.0.0]: https://github.com/jeyeager65/FluidTouch/releases/tag/v1.0.0
