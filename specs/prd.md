---
document_type: product-requirements
product: AgriSense IoT
created: 2026-11-15
updated: 2026-11-15
prd_maturity: planning-ready
---

# Product Requirements Document: AgriSense IoT

> Canonical Product Document

## Product Overview

AgriSense IoT is a low-power IoT platform for home automation: a central ESP-NOW hub bridges battery and mains-powered sensor/actuator nodes to a Python server and Home Assistant, with MQTT discovery and a responsive web dashboard.

## Vision and Why Now

Deliver reliable, low-power home automation that works offline-first over ESP-NOW, avoiding cloud dependency and reducing power consumption.

## Problem Statement

Home automation devices are often cloud-dependent, power-hungry, and hard to integrate. Users need local, low-power sensors/actuators that integrate with Home Assistant without internet.

## Background and Evidence

Based on existing project (homeware): hub v1.2.22, nodes lamp/switch, shared submodule. No formal user research; derived from project history and AGENTS.md rules.

## Target Users, Stakeholders, and Core Personas

### Target Users

- Homeowner/operator wanting local automation
- Technical integrator installing devices

### Stakeholders

- Developer/maintainer (single owner)

### Core Personas

- **Homeowner**: wants reliable local control and energy-efficient sensors.

## User Needs / Jobs To Be Done

- Monitor and control home devices locally
- Deploy battery-powered sensors with months of autonomy
- Integrate with Home Assistant via MQTT discovery

## Product Principles or UX Principles

- **Local-first**: works without internet
- **Low power**: battery nodes deep-sleep

## Scope Summary

Release 1: hub + ESP-NOW nodes + Python server + web dashboard + MQTT/HA integration.

### In-Scope Capabilities

- Hub receiving node data via ESP-NOW and forwarding to server
- Node registration, state reporting, commands
- Web dashboard (hub and nodes)
- MQTT discovery for Home Assistant
- OTA updates

### Out-of-Scope Items

- Cloud connectivity (deferred)
- Mobile app (deferred)

## Product Capability Map

| Capability ID | Capability | Priority | Outcome |
|---------------|------------|----------|---------|
| CAP-001 | Hub bridges ESP-NOW nodes to server/HA | P1 | Central connectivity |
| CAP-002 | Battery node telemetry with deep sleep | P1 | Low-power sensing |
| CAP-003 | Actuator control (relay/lamp/switch) | P1 | Local control |
| CAP-004 | Web dashboard and OTA | P2 | Management |
| CAP-005 | MQTT discovery integration | P1 | HA integration |

## Success Metrics / KPIs / Desired Outcomes

| Metric | Target | Why It Matters | Measurement Window |
|--------|--------|----------------|--------------------|
| Paired devices online | 100% | Connectivity | daily |
| Hub uptime | >99% | Availability | monthly |

## Assumptions

- Single hub per network
- Nodes on same router/AP

## Constraints

- ESP8266/ESP32 memory limits
- Deep sleep wake intervals

## Dependencies

- Home Assistant, MQTT broker, shared submodule

## Risks

- **Hub crash**: intermittent offline; mitigation: watchdog

## Open Questions

- Dual power mode for acquiry-bat node (battery vs USB) — pending implementation decision.

## Release or Validation Approach

Validate via bench testing and field deployment; QC per SDD Pilot.

## Domain Glossary / Terminology

- **Hub**: ESP-NOW receiver bridging to server
- **Node**: sensor/actuator device

## Handoff Guidance

- **Product intent to preserve**: local-first, low-power
- **Scope boundaries to respect**: no cloud in v1
- **Critical constraints**: memory, deep sleep
- **Open decisions needing technical input**: dual power mode for acquiry-bat

## Project Context Baseline Updates

- AgriSense IoT initial PRD (draft)