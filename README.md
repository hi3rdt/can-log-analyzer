# can-log-analyzer

An **AUTOSAR-style MCAL, hand-written at register level**, for the STM32F103C8T6 (Blue Pill, bxCAN), together with **DBC files reverse-engineered from real CAN logs in [SavvyCAN](https://www.savvycan.com/)**. No vehicle and no commercial AUTOSAR tooling are needed: the drivers are unit-tested on a PC against fake registers, and the CAN side is verified with offline log files and SavvyCAN.

## Scope

Two tracks, nothing else:

- **MCAL** on the STM32F103 registers: `Port`, `Dio`, `Mcu`, `Gpt`, `Can` (bxCAN), a `Uart` driver that streams frames to SavvyCAN (LAWICEL/SLCAN), and optionally `Adc`. The drivers use the AUTOSAR SWS API names, take `const` configuration structs, report development errors to `Det`, and access hardware only through register base pointers (`bsw/mcal/reg/Mcal_Reg.h`). The firmware binds those pointers to the real addresses; the host tests bind them to RAM, so every driver is unit-tested on a PC. In production the MCAL comes from the chip vendor; none exists for the F103, which is why it is written here by hand. It is "AUTOSAR-style", not AUTOSAR-compliant: a subset of the APIs, no ARXML configuration, no conformance tests.
- **DBC**: real-vehicle logs are reverse-engineered in SavvyCAN (periods, alive counters, CRC-8 SAE J1850 and sum checksums, physical values) and written down as DBC files in `dbc/`, checked against the log with `tools/check_dbc.py` (cantools). The same DBC later decodes the frames the Blue Pill streams to SavvyCAN.

Out of scope: BSW services (CanIf, PduR, Com, CanTp, Dcm, ...), RTE and application SWCs, because in production they are configured and generated with tools (DaVinci, EB tresos, ISOLAR) that this project does not have.

## DBC track

```sh
pip install -r tools/requirements.txt
python3 tools/check_dbc.py --dbc dbc/gvret_log.dbc --log GVRET_Log.csv
```

`check_dbc.py` reads a SavvyCAN GVRET CSV or a candump log, decodes it with the DBC in strict mode (overlapping signals are errors), and reports per message the frame count, DLC mismatches, the period seen in the log next to `GenMsgCycleTime`, the value range of every signal with out-of-range counts, and whether each `*Counter*` signal steps by +1. The exercise is [docs/exercises/dbc01-gvret-log.vi.md](docs/exercises/dbc01-gvret-log.vi.md).

## Hardware

| Item | Notes |
|---|---|
| STM32F103C8T6 (Blue Pill) | bxCAN remapped to PB8 (RX) / PB9 (TX); USB and CAN share packet SRAM, so the PC link uses UART |
| CAN transceiver | SN65HVD230 (3.3 V) preferred, TJA1050 / MCP2551 also usable |
| USB-UART adapter | CH340 / CP2102, carries the Uart SLCAN stream to SavvyCAN |

No vehicle is required: bring-up uses bxCAN loopback mode, and SavvyCAN reads the frames over the UART link.

## Repository layout

```
bsw/include/           Std_Types.h, Platform_Types.h, ComStack_Types.h, Can_GeneralTypes.h, CanIf_Cbk.h (callbacks the Can driver calls), SchM.h
bsw/mcal/reg/          Mcal_Reg.h: GPIO/RCC register structs and base pointers used by every MCAL driver
bsw/mcal/port/         Port driver: pin mode, clock gating, initial levels (MCAL exercise 1)
bsw/mcal/dio/          Dio driver: channel/port read and atomic BSRR writes (MCAL exercise 1)
bsw/mcal/can/          Can driver interface + Can_Hw pure register-value helpers (MCAL exercise 3)
bsw/services/det/      Default Error Tracer
config/                gateway.dbc: the DBC the demo log is generated from
dbc/                   DBC files reverse-engineered from logs (DBC track, added by the exercises)
tools/                 check_dbc.py (DBC vs. log), demo log generator, tests
platform/stm32f103/    Startup code, linker script, register map, Mcal_RegMap.c (real MCAL base addresses), clock/SysTick setup
app/                   main.c (init sequence, main loop)
cmake/                 arm-none-eabi toolchain file
test/host/             Host unit tests (plain C, run with CTest); stubs/ has fake MCAL registers
samples/               30 s demo drive log (GVRET CSV + candump) generated from config/gateway.dbc
esp32/restbus_sim/     ESP-IDF rest-bus simulator: replays samples/ on CAN with fault injection (Project 1)
tools/openocd/         OpenOCD config for ST-Link + Blue Pill (clone chips included)
docs/                  Roadmap, SavvyCAN guide, exercises (docs/exercises/), setup (docs/setup/), project designs (docs/projects/)
```

Planned modules (`Mcu`, `Gpt`, bxCAN `Can.c`, `Uart`, optional `Adc`) are listed in [docs/roadmap.md](docs/roadmap.md#2-cv-project).

## Build

Host unit tests (gcc, AddressSanitizer + UBSan) and tool tests:

```sh
pip install -r tools/requirements.txt
python3 -m unittest discover -s tools
cmake --preset host && cmake --build --preset host && ctest --preset host
```

The guided exercises in `docs/exercises/` ship header contracts, stub sources and host tests that are switched off until the implementation lands (`-DMCAL_PORT_DIO_EXERCISE_TESTS=ON`, `-DCAN_HW_EXERCISE_TESTS=ON`).

Firmware (GNU Arm toolchain, `arm-none-eabi-gcc`):

```sh
cmake --preset fw && cmake --build --preset fw
# flash with ST-Link + OpenOCD (handles clone Blue Pill chips)
openocd -f tools/openocd/bluepill.cfg -c "program build-fw/firmware.elf verify reset exit"
```

The current firmware brings up the 72 MHz clock tree and blinks the PC13 LED at 1 Hz. Toolchain, VS Code (CMake Tools + Cortex-Debug) and ST-Link setup: [docs/setup/stm32-toolchain.vi.md](docs/setup/stm32-toolchain.vi.md). `CMakePresets.json` has `fw` (firmware) and `host` (unit tests) presets.

## Project 1: instrument cluster on a real CAN bus

The MCAL is put to work in a CAN instrument cluster ECU: the Blue Pill decodes speed, RPM and warning signals with deadline monitoring and alive-counter/checksum checks and drives a 2.4" TFT, while an ESP32 rest-bus simulator ([esp32/restbus_sim](esp32/restbus_sim/README.md)) replays the demo drive log on the bus and injects faults. Design: [docs/projects/p1-instrument-cluster.vi.md](docs/projects/p1-instrument-cluster.vi.md); bench setup: [docs/projects/p1-step1-bringup.vi.md](docs/projects/p1-step1-bringup.vi.md).

## Coding rules

- C99, static allocation only, fixed-width AUTOSAR types, configuration in `const` tables.
- MISRA C:2012 oriented; deviations (integer-to-pointer casts for register addresses, vector table) are confined to `platform/` and commented where they occur. MCAL drivers never use a hard-coded address.
- Warnings are errors (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`).

## Status

- [x] Milestone 1: repo skeleton, toolchain, startup code, Det, host test setup, CI
- [x] Milestone 2: CanIf → PduR → Com with a DBC-to-C generator (removed on 2026-10-11 when the scope was narrowed; see git history)
- [ ] Milestone 3: MCAL base: register layer, Port + Dio, Mcu + Gpt
- [ ] Milestone 4: Can driver for bxCAN (loopback) + Uart SLCAN stream to SavvyCAN
- [ ] Milestone 5: DBC track: DBC for real logs checked with cantools; Blue Pill frames decoded in SavvyCAN
- [ ] Milestone 6: bus-off handling, optional Adc, MISRA report, demo

## License

MIT, see [LICENSE](LICENSE).
