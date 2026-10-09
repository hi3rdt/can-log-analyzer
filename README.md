# can-log-analyzer

Portfolio project for learning automotive embedded software (AUTOSAR Classic–style BSW/ASW layering) and the CAN protocol, built around **offline CAN log files** that are compatible with [SavvyCAN](https://www.savvycan.com/) instead of a real vehicle.

## Goals

- Decode CAN frames and signals from real-car logs (GVRET CSV, candump, ASC) using DBC files.
- Implement DBC signal pack/unpack in C (Intel/Motorola byte order, signed, factor/offset), unit-tested on the host.
- Run a layered CAN stack on an **STM32F103C8T6 (Blue Pill, bxCAN)**: `Can` → `CanIf` → `PduR` → `Com` → `Rte` → SWCs.
- Replay logs into the MCU with original timing and verify the ECU outputs in SavvyCAN.
- Keep the firmware MISRA C:2012 oriented: static allocation only, fixed-width types, config tables in Flash.

## Hardware

| Item | Notes |
|---|---|
| STM32F103C8T6 (Blue Pill) | bxCAN remapped to PB8 (RX) / PB9 (TX); USB and CAN share packet SRAM, so the PC link uses UART |
| CAN transceiver | SN65HVD230 (3.3 V) preferred, TJA1050 / MCP2551 also usable |
| USB-UART adapter | CH340 / CP2102, used for frame logging and log replay |

No vehicle is required: phases 1–3 use bxCAN loopback mode plus log replay.

## Repository layout

```
docs/          Learning roadmap and design notes (EN: roadmap.md, VI: roadmap.vi.md)
```

Planned structure (bsw/, rte/, asw/, tools/, test/) is described in [docs/roadmap.md](docs/roadmap.md#2-cv-project-options).

## Status

Initial setup. See [docs/roadmap.md](docs/roadmap.md) for the phase plan.

## License

MIT, see [LICENSE](LICENSE).
