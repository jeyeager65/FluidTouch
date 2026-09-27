# rmaker_time_sync

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_time_sync/badge.svg)](https://components.espressif.com/components/espressif/rmaker_time_sync)

SNTP-based time synchronization for RainMaker firmware, with optional callbacks when time is updated and helpers for timezone/POSIX timezone strings where applicable.

## Overview

Pass an `esp_rmaker_time_config_t` (or `NULL` for defaults driven by Kconfig) to initialize SNTP, keep system time aligned, and integrate with other components that depend on accurate time or timezone change events.

## Requirements

- ESP-IDF v5.1 or newer

## Dependencies

- `espressif/rmaker_common_events`

See `esp_rmaker_time_sync.h` for initialization, deinitialization, and timezone-related APIs.
