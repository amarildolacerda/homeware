<!-- template-version: 2 -->
# AgriSense IoT Project Instructions

## Core Principles

### I. Loop non-blocking (MUST)

Application code MUST NOT use blocking `delay()` or blocking waits inside `loop()` or any helper called from it. Use `millis()`/`micros()` state machines with independent timers per task. Acceptable blocking only in `setup()` with `yield()`. Rationale: the ESP8266/ESP32 shares the CPU with the Wi-Fi/TCP-IP stack; blocking calls starve it, trigger the software watchdog, and force reboots.

### II. Deep sleep in battery nodes (MUST)

Battery-powered nodes MUST use deep sleep with wake intervals in minutes, keeping active time minimal. `setup()` performs all work; `loop()` is empty. Cache Wi-Fi association (BSSID + channel + static IP) in RTC memory. Never block forever on `WiFi.status()`. Isolate floating GPIOs before sleep. Rationale: active Wi-Fi TX (~120-350 mA) dominates cost; lifetime is governed by wake duration, not sleep current.

### III. Shared as single source of truth (MUST)

Cross-platform logic (protocol, data models, calibration) MUST live in the `shared/` submodule. Platform-specific glue stays in each consumer. Do not duplicate shared code. Bump the shared module deliberately and test on hardware. Rationale: duplicated logic drifts between firmware and server; one definition guarantees message-schema and business-rule consistency.

### IV. MQTT feedback parity (MUST)

Every state change path (command handler, physical input, automation, error fallback) MUST publish the resulting state to the device's state topic. Command and state topics MUST be separate; state retained, commands never retained. Use LWT on an availability topic. State is the single source of truth. Rationale: without parity, dashboards show stale state and retained commands re-execute on reconnect.

### V. Agent Output Style

All agent output MUST be concise and outcome-oriented. This principle supersedes any verbose defaults.

- **Progress reports**: Facts and outcomes only — no narration, no restating the task.
- **Artifacts**: Emit required sections only — no preamble paragraphs, no summary epilogues.
- **Reasoning**: Omit unless the user asks "why" or the decision is non-obvious.
- **Errors / blockers**: State the problem, the attempted fix, and the result — nothing else.
- **Phase-boundary reports**: ≤ 5 bullet points.
- **Preserve without compressing**: Artifact template structure and required sections; explicit decision / registration / validation guidance in shared skills; delegation constraints and sub-agent role definitions; existing size limits (spec ≤ 10 KB, research ≤ 4 KB, stories ≤ 200 words).

## Technology Stack

- **Language/Runtime**: C++17 (ESP8266/ESP32), Python 3.10+ (server)
- **Frameworks**: PlatformIO, Arduino framework, FastAPI/Flask (server)
- **Storage**: EEPROM (nodes), NVS (hub), SQLite/JSON (server)
- **Infrastructure**: Docker (server), ESP-NOW, MQTT, Home Assistant

## Testing & Quality Policy

- **Test Strategy**: Test-after (write tests after implementation)
- **Coverage Target**: 80%
- **Linting/Formatting**: No (manual review)
- **Required QC Categories**: coverage
- **Security Scanning**: No

## Source Code Layout

- **Policy**: PRESERVE_EXISTING_LAYOUT (existing repository; no /src-only rule)
- **Source root**: hub/, nodes/, server/, shared/

## Development Workflow

- **Branching**: Feature branches + squash merge (dev → main via PR)
- **Commit Convention**: Conventional Commits (feat:, fix:, etc.)
- **Review Process**: Manual review before merge to dev

## Governance

- **Version**: 1.0.0
- **Last Amended**: 2026-10-26
- **Amendment Procedure**: Update this file; bump version per semver.

<!-- template-version: 2 -->