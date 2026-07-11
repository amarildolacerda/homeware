# Lampada Repeater Statistics Design

## Goal

Add repeater statistics to the lampada client (esp8266_lampada) so users can see which terminals used it as a repeater and how many packets were retransmitted.

## Context

The lampada client has a repeater mode (`s_use_repeater`) that forwards packets between clients and gateway. Currently this mode has zero tracking - no counters, no client list, no visibility into repeater activity.

## Design

### Data Structures (main.cpp)

```c
#define MAX_REPEATER_CLIENTS 5

typedef struct {
    uint8_t mac[6];
    uint32_t pkt_count;
} repeater_client_t;

static repeater_client_t s_rep_clients[MAX_REPEATER_CLIENTS];
static int s_rep_client_num = 0;
static uint32_t s_repeater_fwd = 0;
```

### Tracking Logic (in espnow_recv_cb)

When `s_use_repeater` is active and a packet is forwarded:

1. Increment `s_repeater_fwd`
2. Call `track_repeater_client(mac)` to track the source MAC
3. If MAC already tracked, increment its `pkt_count`
4. If new MAC and slot available, add to array
5. If array full, ignore new client (keep existing counts)

```c
static void track_repeater_client(const uint8_t *mac) {
    for (int i = 0; i < s_rep_client_num; i++) {
        if (memcmp(s_rep_clients[i].mac, mac, 6) == 0) {
            s_rep_clients[i].pkt_count++;
            return;
        }
    }
    if (s_rep_client_num < MAX_REPEATER_CLIENTS) {
        memcpy(s_rep_clients[s_rep_client_num].mac, mac, 6);
        s_rep_clients[s_rep_client_num].pkt_count = 1;
        s_rep_client_num++;
    }
}
```

### API Extension (/api/state)

Add to existing JSON response:

```json
{
  "...existing fields...",
  "repeater_active": true,
  "repeater_fwd": 123,
  "repeater_clients": [
    {"mac": "AA:BB:CC:DD:EE:FF", "packets": 45},
    {"mac": "11:22:33:44:55:66", "packets": 78}
  ]
}
```

- `repeater_active`: boolean, true when `s_use_repeater` is enabled
- `repeater_fwd`: total packets forwarded through this device
- `repeater_clients`: array of objects with MAC and packet count (max 5)

### Dashboard (pages.h)

Add "Repeater" item in the existing sidebar navigation (only shown when `repeater_active = true`):

- New nav-item in sidebar: "📡 Repeater"
- New section `secRepeater` with:
  - Badge showing total forwarded packets
  - Table with columns: MAC | Pacotes
  - Styled consistently with existing dashboard
- Sidebar item hidden when `repeater_active = false`

### Serial Commands

Add `p` command to print repeater stats:
```
--- Repeater Stats ---
  Forwarded: 123
  Clients:   2
  0: AA:BB:CC:DD:EE:FF (45 pkts)
  1: 11:22:33:44:55:66 (78 pkts)
----------------------
```

Add `c` command to also reset repeater counters.

## Memory Impact

- `repeater_client_t` = 10 bytes
- 5 clients = 50 bytes extra
- Total acceptable for ESP8266

## Files to Modify

- `clients/esp8266_lampada/src/main.cpp` - add tracking logic and API fields
- `clients/esp8266_lampada/include/pages.h` - add dashboard section

## Out of Scope

- Direction tracking (client→GW vs GW→client) - would double memory
- Gateway-side awareness of repeater usage - requires protocol change
- Persistent storage of repeater stats - not needed for runtime visibility
