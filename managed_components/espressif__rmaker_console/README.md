# rmaker_console

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_console/badge.svg)](https://components.espressif.com/components/espressif/rmaker_console)

Serial console integration for ESP RainMaker: initializes the console and registers common RainMaker CLI commands.

## Overview

Call `esp_rmaker_common_console_init()` for a one-shot setup, or initialize the console yourself and then call `esp_rmaker_common_register_commands()` to add the shared RainMaker commands only.

## Requirements

- ESP-IDF v5.1 or newer

## Dependencies

- `espressif/rmaker_system_ctrl`
- `espressif/rmaker_time_sync`

See `esp_rmaker_common_console.h` for initialization notes and extension points.
