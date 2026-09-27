# rmaker_cmd_resp

Encode and decode RainMaker command/response payloads using a TLV-style binary format (request id, user role, status, timestamp, command, data).

## Overview

Use this component when implementing the device side of RainMaker remote commands: build outgoing responses or parse incoming command frames in a consistent way with the rest of the RainMaker stack.

## Requirements

- ESP-IDF v5.1 or newer

## Dependencies

- None (ESP-IDF only)

See `esp_rmaker_cmd_resp.h` for TLV types, role flags, and API entry points.
