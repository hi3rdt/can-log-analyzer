# can-log-analyzer

An **AUTOSAR-style MCAL, hand-written at register level**, plus an AUTOSAR Classic–inspired **CAN communication stack** on top of it, for the STM32F103C8T6 (Blue Pill, bxCAN). Written without commercial AUTOSAR tooling and verified with **offline CAN log files** that are compatible with [SavvyCAN](https://www.savvycan.com/) instead of a real vehicle.

## Scope

Only Basic Software (BSW) is implemented, in two parts:

- **MCAL** on the STM32F103 registers: `Port`, `Dio`, `Mcu`, `Gpt`, `Can` (bxCAN) and optionally `Adc`. The drivers use the AUTOSAR SWS API names, take `const` configuration structs, report development errors to `Det`, and access hardware only through register base pointers (`bsw/mcal/reg/Mcal_Reg.h`). The firmware binds those pointers to the real addresses; the host tests bind them to RAM, so every driver is unit-tested on a PC. In production the MCAL comes from the chip vendor; none exists for the F103, which is why it is written here by hand. It is "AUTOSAR-style", not AUTOSAR-compliant: a subset of the APIs, no ARXML configuration, no conformance tests.
- **CAN communication stack**: `Can` → `CanIf` → `PduR` → `Com`, plus `Det`. RTE and application SWCs are intentionally out of scope, because in production they come from configuration tools (DaVinci, EB tresos, ISOLAR). The ECU acts as a **CAN gateway**: PDU routing in PduR and signal routing in Com, with deadline monitoring and periodic transmission. The public Com API (`Com_ReceiveSignal` / `Com_SendSignal`) is the upper boundary.

Configuration tables (`*_Cfg.c`) are generated from DBC files by a small Python tool (`tools/dbc2c.py`), mirroring what a configuration tool does.

## Data flow

```
 CAN frame --> Can (ISR) --> CanIf_RxIndication --> PduR_CanIfRxIndication --+--> Com_RxIndication (copy bytes)
                             software filter,       routing table, 1:n       |       |
                             DLC check                                       |       v
                                                                             |   Com_ReceiveSignal (task: unpack, sign-extend)
                                                                             |
                                                                             +--> CanIf_Transmit (PDU gateway, new CAN ID)

 Com_SendSignal (pack) --> Com_TriggerIPDUSend --> PduR_ComTransmit --> CanIf_Transmit --> Can_Write
```

## Configuration from DBC

`config/gateway.dbc` describes the bus, `config/routes.json` names the ECU node and the PDU gateway routes:

```sh
python3 tools/dbc2c.py --dbc config/gateway.dbc --routes config/routes.json --out gen
```

For node `GW` this creates:

- Com RX I-PDUs for every message that has a signal received by `GW`, and Com TX I-PDUs for every message `GW` transmits.
- CanIf RX entries sorted by CAN ID (binary search), with a DLC check, plus CanIf TX entries.
- PduR routes. A PDU gateway target is a pure CanIf L-PDU, not a Com I-PDU.
- Symbolic handles such as `ComConf_ComSignal_EngineRpm` and `ComConf_ComIPdu_GW_VehicleState`.

The generator rejects overlapping signals, signals outside the DLC, multiplexed signals, CAN FD and signals wider than 32 bits. The generated files in `gen/` are committed so they can be reviewed, and CI fails if they are out of date.

## Verification against cantools

`tools/gen_test_vectors.py` uses [cantools](https://github.com/cantools/cantools) as an independent DBC implementation. It decodes random frames and encodes random signal values, and `test/host/test_comstack.c` checks that the C stack produces the same raw values and bytes for every signal. That covers Intel and Motorola byte order, signed and unsigned signals, and standard and extended IDs.

## Hardware

| Item | Notes |
|---|---|
| STM32F103C8T6 (Blue Pill) | bxCAN remapped to PB8 (RX) / PB9 (TX); USB and CAN share packet SRAM, so the PC link uses UART |
| CAN transceiver | SN65HVD230 (3.3 V) preferred, TJA1050 / MCP2551 also usable |
| USB-UART adapter | CH340 / CP2102, used for frame logging and log replay |

No vehicle is required: bring-up uses bxCAN loopback mode plus log replay.

## Repository layout

```
bsw/include/           Std_Types.h, Platform_Types.h, ComStack_Types.h, Can_GeneralTypes.h, SchM.h
bsw/mcal/reg/          Mcal_Reg.h: GPIO/RCC register structs and base pointers used by every MCAL driver
bsw/mcal/port/         Port driver: pin mode, clock gating, initial levels (MCAL exercise 1)
bsw/mcal/dio/          Dio driver: channel/port read and atomic BSRR writes (MCAL exercise 1)
bsw/mcal/can/          Can driver interface + Can_Hw pure register-value helpers (MCAL exercise 3)
bsw/ecual/canif/       CAN Interface: software filter, DLC check, ID <-> PDU mapping
bsw/services/pdur/     PDU Router: Com/CanIf routing, PDU gateway
bsw/services/com/      COM: signal pack/unpack (Intel/Motorola, signed), I-PDU send
bsw/services/det/      Default Error Tracer
config/                Bus DBC and gateway routes (input of the generator)
gen/                   Generated CanIf/PduR/Com configuration (do not edit)
tools/                 dbc2c.py generator, cantools test-vector generator, demo log generator, tests
platform/stm32f103/    Startup code, linker script, register map, Mcal_RegMap.c (real MCAL base addresses), clock/SysTick setup
app/                   main.c (init sequence, main loop)
cmake/                 arm-none-eabi toolchain file
test/host/             Host unit tests (plain C, run with CTest); stubs/ has fake MCAL registers
samples/               30 s demo drive log (GVRET CSV + candump) generated from config/gateway.dbc
docs/                  Roadmap, SavvyCAN hands-on guide, guided exercises (docs/exercises/)
```

Planned modules (`Mcu`, `Gpt`, bxCAN `Can.c`, optional `Adc`, `cdd/uartgw`, Os scheduler) are listed in [docs/roadmap.md](docs/roadmap.md#2-cv-project).

## Build

Host unit tests (gcc, AddressSanitizer + UBSan) and generator tests:

```sh
pip install -r tools/requirements.txt
python3 -m unittest discover -s tools
cmake -S . -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

The guided exercises in `docs/exercises/` ship header contracts, stub sources and host tests that are switched off until the implementation lands (`-DMCAL_PORT_DIO_EXERCISE_TESTS=ON`, `-DCAN_HW_EXERCISE_TESTS=ON`).

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
- MISRA C:2012 oriented; deviations (integer-to-pointer casts for register addresses, vector table) are confined to `platform/` and commented where they occur. MCAL drivers never use a hard-coded address.
- Warnings are errors (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`).

## Status

- [x] Milestone 1: repo skeleton, toolchain, startup code, Det, host test setup, CI
- [x] Milestone 2: CanIf → PduR → Com with `dbc2c.py` configuration generator
- [ ] Milestone 3: MCAL base: register layer, Port + Dio, Mcu + Gpt
- [ ] Milestone 4: Can MCAL driver for bxCAN (loopback) + UART frame gateway
- [ ] Milestone 5: Os scheduler, periodic TX, deadline monitoring, signal gateway, log replay
- [ ] Milestone 6: bus-off recovery, optional Adc, MISRA report, demo

## License

MIT, see [LICENSE](LICENSE).
