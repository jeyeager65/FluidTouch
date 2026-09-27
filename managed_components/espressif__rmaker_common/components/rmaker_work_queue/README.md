# rmaker_work_queue

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_work_queue/badge.svg)](https://components.espressif.com/components/espressif/rmaker_work_queue)

Dedicated-thread work queue for ESP RainMaker: queue callbacks to run sequentially on a single worker task.

## Overview

Initialize the queue, optionally start it, then enqueue work with `esp_rmaker_work_queue_add_task()`. Work runs in the worker thread context (not ISR-safe unless documented otherwise for your usage).

## Requirements

- ESP-IDF v5.1 or newer

## Typical flow

1. `esp_rmaker_work_queue_init()`
2. `esp_rmaker_work_queue_start()`
3. Submit work with `esp_rmaker_work_queue_add_task()`
4. `esp_rmaker_work_queue_stop()` then `esp_rmaker_work_queue_deinit()` on shutdown

See `esp_rmaker_work_queue.h` for full API.

## Dependencies

None beyond ESP-IDF.
