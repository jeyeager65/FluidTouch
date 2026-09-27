# rmaker_system_ctrl

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_system_ctrl/badge.svg)](https://components.espressif.com/components/espressif/rmaker_system_ctrl)

High-level device control APIs for RainMaker: delayed reboot, Wi-Fi credential reset, factory reset, and related operations that emit common RainMaker events.

## Overview

These helpers perform resets and reboots asynchronously where appropriate so other subsystems (for example MQTT) can flush state before the device restarts. Event IDs such as `RMAKER_EVENT_REBOOT` are raised as described in the API documentation.

## Requirements

- ESP-IDF v5.1 or newer

## Dependencies

None beyond ESP-IDF.

See `esp_rmaker_system_ctrl.h` for parameters and behavior of each API.
