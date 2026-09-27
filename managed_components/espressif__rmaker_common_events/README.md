# rmaker_common_events

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_common_events/badge.svg)](https://components.espressif.com/components/espressif/rmaker_common_events)

Defines the shared ESP RainMaker event base (`RMAKER_COMMON_EVENT`) and the common event IDs that the other RainMaker components post on.

## Overview

This component owns a single `esp_event` base that is shared across the RainMaker stack. Other components (e.g. `rmaker_system_ctrl`, `rmaker_time_sync`) and the top-level `rmaker_common` post events on this base; applications subscribe to it via the standard `esp_event_handler_register()` API.

```c
#include <esp_rmaker_common_events.h>

esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &handler, NULL);
```

## Events

The events in `esp_rmaker_common_event_t` (see `esp_rmaker_common_events.h` for the associated event data of each):

| Event | Raised when |
|-------|-------------|
| `RMAKER_EVENT_REBOOT` | A delayed reboot has been triggered (data: seconds, `uint8_t`) |
| `RMAKER_EVENT_WIFI_RESET` | Wi-Fi credentials reset (`esp_rmaker_wifi_reset()`) |
| `RMAKER_EVENT_FACTORY_RESET` | Factory reset (`esp_rmaker_factory_reset()`) |
| `RMAKER_MQTT_EVENT_CONNECTED` | Connected to the MQTT broker |
| `RMAKER_MQTT_EVENT_DISCONNECTED` | Disconnected from the MQTT broker |
| `RMAKER_MQTT_EVENT_PUBLISHED` | MQTT message published (data: message ID, `int`) |
| `RMAKER_EVENT_TZ_POSIX_CHANGED` | POSIX timezone changed (data: POSIX TZ string) |
| `RMAKER_EVENT_TZ_CHANGED` | Timezone changed (data: timezone string) |
| `RMAKER_MQTT_EVENT_MSG_DELETED` | MQTT message dropped from the outbox (data: message ID, `int`) |

> Note: the enum values are positional. Append new events at the end to preserve binary/ABI compatibility for existing users.

## Why this is a separate component

The goal of the wider refactor is to break `rmaker_common` into individual, independently-reusable sub-components. However, `rmaker_common` has always exposed a _single_ event base, `RMAKER_COMMON_EVENT`, that applications register a handler on, and existing users subscribe to it for reboot, reset, MQTT and timezone events. To keep that public contract working after the split, the common event base has to be preserved.

Since the base is now posted on by several independent sub-components (`rmaker_system_ctrl`, `rmaker_time_sync`) as well as `rmaker_common` itself, it needs a single common home that all of them can depend on. This component is that home. Keeping it separate:

* preserves backward compatibility — current users keep subscribing to `RMAKER_COMMON_EVENT` exactly as before;
* avoids a duplicate-symbol link error that would result from defining the base (`ESP_EVENT_DEFINE_BASE`) in more than one component; and
* avoids a circular dependency — the base cannot live in the top-level `rmaker_common`, because the sub-components that post on it would then have to depend back on `rmaker_common`, which already depends on them.

The component is intentionally small — it is a shared compatibility contract, not a feature.

## Future direction

As the sub-components mature towards standalone reuse, each may define and post on its own event base (e.g. a time-sync component owning only its timezone events), which reads more naturally for a consumer that uses just that component. If/when that happens, `RMAKER_COMMON_EVENT` will be retained as the backward-compatible base, with the per-component bases offered alongside it and the common events documented as deprecated, to be removed only at a future major version. This is intentionally out of scope for the initial split.

## Requirements

* ESP-IDF v5.1 or newer

## Dependencies

None beyond ESP-IDF (`esp_event`).
