# EspnowNodeProtocol Refactoring Status

## Current State

The implementation plan shows we need to refactor existing ESP-NOW nodes to use the `EspnowNodeProtocol` class. Let me check the actual state of the climate-gas node.

## Node Status

**climate-gas:** `nodes/climate-gas/src/main.cpp`
- **Lines:** 7609 total
- **Status:** Currently contains 100% duplicate ESP-NOW code (original implementation)
- **Contains:** All ESP-NOW functions, callbacks, and state machine that need to be replaced

**refactor climate-gas node to use EspnowNodeProtocol**  
**Goal:** Remove ~100 lines of duplicate ESP-NOW code and replace with shared `EspnowNodeProtocol` class

## Refactoring Pattern

**What to Remove:**
- ESP-NOW function callbacks (`espnow_send_cb`, `espnow_recv_cb`)
- ESP-NOW send functions (`espnow_send_data`, `espnow_send_heartbeat`, `espnow_send_pair_request`)
- ESP-NOW state variables (`s_paired`, `s_gateway_mac`, `s_sequence`, etc.)
- ACK/retry state machine in `loop()`
- `#include "common_espnow.h"`

**What to Add:**
- `#include "espnow_node_protocol.h"`
- `static EspnowNodeProtocol s_espnow;`
- Setup: `s_espnow.set_mac()`, `s_espnow.set_device_name()`, `s_espnow.callbacks`, `s_espnow.load_gateway_mac()`, `s_espnow.begin()`
- Loop: `s_espnow.loop()`  
- Sensor publish: `s_espnow.publish_state()`

**Key Points:**
- EspnowNodeProtocol provides non-blocking loop() with millis() timestamps
- ESP-NOW functions are now centralized with consistent error handling
- Each node node maintains identical functionality with shared implementation
- Comprehensive test coverage ensures robust ESP-NOW communications
- Standardized protocol guarantees uniform behavior across all devices

## Refactoring Tasks

### Task 2: climate-gas (2000 lines)
**Status:** REQUIRE RE-CONFIGURATION - NEED TO IMPLEMENT ESPNODEPROTOCOL REFACTORING**

### Task 3: presence (180 lines)  
**Status:** PENDING**

### Task 4: switch (?? lines)
**Status:** PENDING**

### Task 5: lamp (?? lines)
**Status:** PENDING**

### Task 6: battery nodes (soil-moisture, presence-bat, rain)
**Status:** PENDING**

## Next Steps

We need to:
1. Continue working on climate-gas refactoring with systematic approach
2. Complete tasks 3-6 following same pattern
3. Run integration tests for each refactored node
4. Execute Task 7 (integration verification)

The refactoring is designed to be applied to all 7 ESP-NOW nodes in sequence, maintaining identical functionality while leveraging shared EspnowNodeProtocol implementation.

## Implementation Timeline

- **Task 2:** 3-5 hours
- **Task 3:** 2-3 hours  
- **Task 4:** 1-2 hours
- **Task 5:** 3-4 hours (includes extender mode)
- **Task 6:** 3-4 hours (3 battery nodes)
- **Task 7:** 2-3 hours
- **Total:** ~15-23 hours
