# can-log-analyzer

An AUTOSAR Classic–inspired **BSW CAN communication stack** for the STM32F103C8T6 (Blue Pill, bxCAN), written without commercial AUTOSAR tooling and verified with **offline CAN log files** that are compatible with [SavvyCAN](https://www.savvycan.com/) instead of a real vehicle.

## Scope

Only the Basic Software (BSW) communication path is implemented: `Can` → `CanIf` → `PduR` → `Com`, plus `Det`. RTE and application SWCs are intentionally out of scope, because in production they come from configuration tools (DaVinci, EB tresos, ISOLAR). The ECU acts as a **CAN gateway**: PDU routing in PduR and signal routing in Com, with deadline monitoring and periodic transmission. The public Com API (`Com_ReceiveSignal` / `Com_SendSignal`) is the upper boundary.

Configuration tables (`*_Cfg.c`) are generated from DBC files by a small Python tool (`tools/dbc2c.py`, coming in the next milestone), mirroring what a configuration tool does.

## Hardware

| Item | Notes |
|---|---|
| STM32F103C8T6 (Blue Pill) | bxCAN remapped to PB8 (RX) / PB9 (TX); USB and CAN share packet SRAM, so the PC link uses UART |
| CAN transceiver | SN65HVD230 (3.3 V) preferred, TJA1050 / MCP2551 also usable |
| USB-UART adapter | CH340 / CP2102, used for frame logging and log replay |

No vehicle is required: bring-up uses bxCAN loopback mode plus log replay.

## Repository layout

```
bsw/include/           Std_Types.h, Platform_Types.h, ComStack_Types.h
bsw/services/det/      Default Error Tracer
platform/stm32f103/    Startup code, linker script, register map, clock/SysTick setup
app/                   main.c (init sequence, main loop)
cmake/                 arm-none-eabi toolchain file
test/host/             Host unit tests (plain C, run with CTest)
docs/                  Learning roadmap (EN: roadmap.md, VI: roadmap.vi.md)
```

Planned modules (`bsw/mcal/can`, `bsw/ecual/canif`, `bsw/services/pdur`, `bsw/services/com`, `cdd/uartgw`, `tools/`) are listed in [docs/roadmap.md](docs/roadmap.md#2-cv-project).

## Build

Host unit tests (gcc, AddressSanitizer + UBSan):

```sh
cmake -S . -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Firmware (GNU Arm toolchain, `arm-none-eabi-gcc`):

```sh
cmake -S . -B build-fw -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-fw
# flash with ST-Link
st-flash write build-fw/firmware.bin 0x08000000
```

The current firmware brings up the 72 MHz clock tree and blinks the PC13 LED at 1 Hz.

## Coding rules

- C99, static allocation only, fixed-width AUTOSAR types, configuration in `const` tables.
- MISRA C:2012 oriented; deviations (register access, vector table) are confined to `platform/` and commented where they occur.
- Warnings are errors (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`).

## Status

- [x] Milestone 1: repo skeleton, toolchain, startup code, Det, host test setup, CI
- [ ] Milestone 2: CanIf → PduR → Com with `dbc2c.py` configuration generator
- [ ] Milestone 3: bxCAN driver (loopback) + UART frame gateway
- [ ] Milestone 4: Os scheduler, periodic TX, deadline monitoring, signal gateway, log replay
- [ ] Milestone 5: bus-off recovery, MISRA report, demo

## License

MIT, see [LICENSE](LICENSE).
