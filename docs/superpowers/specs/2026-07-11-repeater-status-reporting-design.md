# Repeater Status Reporting — Design Spec

## Goal

Make the repeater visible on the gateway by sending its operational stats (RX/TX, client count, radio status, ACK failures) via ESP-NOW every 30s. The gateway registers the repeater as a `SENSOR_TYPE_REPEATER` and displays it on the dashboard.

## Motivation

Currently the repeater is invisible to the gateway — it only relays packets transparently. The gateway cannot tell if a repeater is online, how many clients are behind it, or if it's experiencing issues. This feature adds repeater telemetry so the gateway operator has full visibility into the mesh.

## Architecture

### Protocol Extension

Add `ESPNOW_MSG_REPEATER_STATUS = 0x0B` to the ESP-NOW message type enum (both gateway and repeater protocol headers).

Add `SENSOR_TYPE_REPEATER = 9` to the sensor type enum.

Define a new packed payload struct:

```c
typedef struct __attribute__((packed)) {
    uint16_t received;       // ESP-NOW packets received from clients
    uint16_t forwarded;      // ESP-NOW packets forwarded to gateway
    uint8_t  client_count;   // number of active client peers
    uint8_t  channel;        // WiFi channel
    int16_t  rssi;           // RSSI to gateway
    uint32_t uptime_s;       // uptime in seconds
    uint16_t free_heap;      // free heap bytes
    uint8_t  ack_failures;   // ESP-NOW send failures
} payload_repeater_status_t; // 16 bytes
```

Uses the standard `espnow_header_t` wrapper with `sensor_type = SENSOR_TYPE_REPEATER`. The gateway identifies the repeater by its `sensor_mac` field (the repeater's own MAC address).

### Repeater Side (Sender)

- Timer: every 30s (non-blocking, uses `millis()`)
- Sends `ESPNOW_MSG_REPEATER_STATUS` via `esp_now_send()` to `s_gateway_mac`
- Uses the standard `espnow_header_t` with version, sequence, sensor_mac (own MAC), sensor_type, battery_pct (0 for repeater), rssi, and the `payload_repeater_status_t` payload
- Only sends when `s_gateway_configured == true` and gateway MAC is valid
- Sequence number incremented per send

### Gateway Side (Receiver)

- `espnow_recv_cb()`: new case for `ESPNOW_MSG_REPEATER_STATUS`
  - Validates length >= `ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_repeater_status_t)`
  - Finds or creates slot via `sensor_registry_find_by_mac()` / `sensor_registry_find_free_slot()`
  - Calls `sensor_registry_update_state()` which deserializes the payload into the `repeater` union member
  - Sends ACK (same as SENSOR_DATA pattern)
  - Queues MQTT state publish

### Sensor Registry Extension

Add to `virtual_sensor_t` union:
```c
struct {
    uint16_t received;
    uint16_t forwarded;
    uint8_t  client_count;
    uint8_t  channel;
    uint32_t uptime_s;
    uint16_t free_heap;
    uint8_t  ack_failures;
} repeater;
```

Add case in `sensor_registry_update_state()` to deserialize `payload_repeater_status_t` into the union.

### Web Server Extension

Add `SENSOR_TYPE_REPEATER` case in `/api/sensors` JSON builder to include repeater-specific fields in the `state` sub-object.

### Dashboard Extension

Add a "Repeater" section to the gateway dashboard (`pages.h`) showing:
- Device name, MAC, online/offline status
- RX · TX · Clients · ACK Failures
- Channel · RSSI · Uptime · Free Heap

## Files to Modify

| File | Change |
|------|--------|
| `gateway/include/espnow_protocol.h` | Add `ESPNOW_MSG_REPEATER_STATUS = 0x0B`, `SENSOR_TYPE_REPEATER = 9`, `payload_repeater_status_t` |
| `gateway/src/espnow_handler.cpp` | Add case `0x0B` in `espnow_recv_cb()` |
| `gateway/src/sensor_registry.cpp` | Add repeater union member, add case in `update_state()` |
| `gateway/src/web_server.cpp` | Add REPEATER case in `/api/sensors` |
| `gateway/include/pages.h` | Add repeater section to dashboard |
| `clients/esp8266_repeater/include/espnow_protocol.h` | Add `0x0B`, `SENSOR_TYPE_REPEATER = 9`, `payload_repeater_status_t` |
| `clients/esp8266_repeater/src/main.cpp` | Add 30s timer + `send_repeater_status()` function |

## Constraints

- ESP-NOW frame max 250 bytes — payload is 16 bytes, well within limit
- Repeater does NOT pair like a sensor — it already discovers the gateway via `GW_DISCOVER`. The gateway auto-registers it on first `REPEATER_STATUS` receive (like SENSOR_DATA for unknown MACs)
- No EEPROM layout change — repeater status is transient (updated in RAM only, same as sensor state)
- Non-blocking: timer uses `millis()`, no `delay()` in the send path
- Both protocol headers (gateway + repeater) must stay in sync

## Testing

1. Build gateway: `pio run -d gateway`
2. Build repeater: `pio run -d clients/esp8266_repeater`
3. Verify gateway receives REPEATER_STATUS: check serial log for registration
4. Verify dashboard shows repeater section with live stats
5. Verify stats update every 30s
6. Verify offline detection when repeater goes silent (5min timeout)
