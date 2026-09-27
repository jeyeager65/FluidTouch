# ESP RainMaker Common modules

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_common/badge.svg)](https://components.espressif.com/components/espressif/rmaker_common)

This component consists of some common modules used by ESP RainMaker and ESP RainMaker Diagnostics repos.
Currently, it consists of
- MQTT glue layer
- Factory NVS helpers
- [Command-Response](./components/rmaker_cmd_resp/)
- [Console](./components/rmaker_console/)
- Memory allocation helpers (`esp_rmaker_mem_alloc.h`, using SPIRAM etc.)
- [System control](./components/rmaker_system_ctrl/) (Reboot, Wi-Fi/factory reset etc.)
- [Timing APIs](./components/rmaker_time_sync/) (SNTP helpers, timezone, etc.)
- [Work Queue](./components/rmaker_work_queue/)

## Pre-commit Checks

File checks are achieved with [pre-commit](https://pre-commit.com) hooks [here](.pre-commit-config.yaml):
- Using [insert-license](https://github.com/Lucas-C/pre-commit-hooks?tab=readme-ov-file#insert-license) for license headers
- Using [end-of-file-fixer](https://github.com/pre-commit/pre-commit-hooks?tab=readme-ov-file#end-of-file-fixer) to ensure end-of-file newline compliance

### Setup

In your Python environment, run

```bash
pip install pre-commit
```

### Usage

To install the hooks to run before every commit is finalized:

```bash
pre-commit install
```

To run on all files (not just staged), at any point of time:

```bash
pre-commit run --all-files
```
