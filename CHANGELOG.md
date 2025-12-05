# Changelog

All notable changes to FluidTouch will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

[1.0.1]: https://github.com/jeyeager65/FluidTouch/releases/tag/v1.0.1
[1.0.0]: https://github.com/jeyeager65/FluidTouch/releases/tag/v1.0.0
