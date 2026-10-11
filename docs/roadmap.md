# Learning Roadmap & CV Project — AUTOSAR-style MCAL + CAN stack on STM32F103

Owner: hi3rdt · Date: 2026-10-09 · Updated: 2026-10-11 (MCAL + CAN stack)
Hardware: STM32F103C8T6 (Blue Pill, bxCAN) · Tooling: SavvyCAN, CAN log files (offline)

---

## 0. Hardware facts that shape the whole plan

| Constraint | Impact | Decision |
|---|---|---|
| On F103, **USB and bxCAN share the same 512-byte packet SRAM** (RM0008) | You cannot run USB-CDC and CAN at the same time | PC link goes over **UART** via a USB-UART adapter (CH340/CP2102), not native USB |
| Default CAN pins PA11/PA12 are the USB pins | Conflict with USB D-/D+ pull-up on Blue Pill (R10 1.5k on PA12) | Remap CAN to **PB8 (RX) / PB9 (TX)** (`AFIO_MAPR CAN_REMAP = 0b10`) |
| bxCAN needs an external transceiver | No bus without one | **SN65HVD230** (3.3 V) preferred; TJA1050 (5 V) works because PB8/PB9 are 5 V tolerant, but needs a 5 V rail |
| Only one node + log files | No real bus partner | Use bxCAN **Loopback / Silent-Loopback** mode in phase 1–3; add a 2nd node later (optional) |
| 20 KB RAM / 64 KB Flash | Static allocation only | No `malloc`, fixed-size PDU buffers, config tables in `const` (Flash) |

---

## 1. Roadmap (≈10 weeks, ~8–10 h/week)

### Phase 1 — CAN fundamentals with SavvyCAN (week 1–2)
- Frame anatomy: SOF, 11/29-bit ID, RTR, DLC, data, CRC, ACK, EOF; bit stuffing; arbitration; error frames and the TEC/REC fault confinement (Error Active → Passive → Bus-Off).
- Bit timing: TQ, SYNC/PROP/PHASE1/PHASE2, sample point (~87.5% for 500 kbit/s in automotive).
- SavvyCAN hands-on with public logs (no hardware needed):
  - Open a log (GVRET CSV / candump / ASC), use **Frame Info**, **Graphing**, **Flow View**, **Sniffer**.
  - Load a **DBC**, decode signals, plot e.g. vehicle speed vs. time.
  - Reverse-engineer one unknown signal from a log (find a counter, a checksum, a changing byte).
- Deliverable: short notes + screenshots of 3 decoded signals (goes into the project README later).
- Sources of logs/DBC: `commaai/opendbc` (DBC files for real cars), `linklayer/cantools` test data, SavvyCAN sample logs. Pick one car brand with both a DBC and a log.

### Phase 2 — DBC & signal math in C (week 2–3)
- Intel (little-endian) vs. Motorola (big-endian) bit numbering — the #1 source of decode bugs.
- Physical = raw × factor + offset; signed signals; min/max; multiplexed signals.
- Write a host-side C library `CanSig_Unpack/Pack` + unit tests (Unity or CppUTest) on PC, validate against `cantools` (Python) output for the same log.

### Phase 3 — Register-level MCAL on STM32: Port/Dio, Mcu/Gpt, then bxCAN (week 3–5)
- GPIO first (exercise 1): CRL/CRH 4-bit CNF|MODE per pin, IDR/ODR, atomic BSRR, RCC clock gating. Then Mcu (HSE → PLL 72 MHz, flash wait states) and Gpt (TIM2 as a 1 ms tick).
- Clock tree (HSE 8 MHz → 72 MHz, APB1 = 36 MHz), bit timing for 500 kbit/s: prescaler 4, BS1 = 15, BS2 = 2, SJW = 1 → 36 MHz / 4 / 18 TQ = 500 kbit/s, sample point 16/18 = 88.9%.
- Init/sleep/normal mode transitions (INRQ/INAK, SLEEP/SLAK), loopback mode for self-test.
- 3 TX mailboxes, 2 RX FIFOs (3 deep each), 14 filter banks: ID-list vs. mask mode.
- Interrupt-driven RX (USB_LP_CAN1_RX0 IRQ), TX-complete, error/bus-off handling (ABOM).
- UART logging of every frame in **GVRET-like / SLCAN text** so SavvyCAN or a Python script can capture it.
- Deliverable: STM32 in loopback receives its own frames; SavvyCAN shows them.

### Phase 4 — AUTOSAR concepts mapped onto your code (week 4–6, overlaps)
- Layered architecture: MCAL → ECU Abstraction → Services → RTE → SWC. Know all layers, **implement only the BSW part** (MCAL, ECU Abstraction, Services).
- Communication stack path: `Can` → `CanIf` → `PduR` → `Com` → (RTE → SWC in a real ECU). In this project the Com public API (`Com_ReceiveSignal` / `Com_SendSignal`) is the upper boundary.
- Concepts to actually implement (simplified): PDU, I-PDU vs. L-PDU, signal/PDU configuration tables, Com TX modes (periodic / direct / mixed), deadline monitoring (timeout → substitute value), update bits, PduR PDU gateway, Com signal gateway.
- Read-only theory (no implementation needed for CV): RTE and SWC/runnables, EcuM/BswM, NvM/MemIf, Dcm/UDS basics, E2E protection profiles (CRC + alive counter).
- Reference: AUTOSAR Classic SWS documents for Can, CanIf, PduR, Com (free on autosar.org) — read only API + config sections.

### Phase 5 — CV project build (week 6–10)
See section 2. Final week: README, architecture diagram, demo video/GIF from SavvyCAN, MISRA report.

---

## 2. CV project

### Scope update (2026-10-11): MCAL + CAN stack
The project now also owns the layer below CanIf: a hand-written **AUTOSAR-style MCAL** on the STM32F103 registers, in this order: Port + Dio (GPIO), Mcu (clock tree) + Gpt (hardware timer for the OS tick), Can (bxCAN), and optionally Adc. Every driver uses the SWS API names (`Port_Init`, `Dio_WriteChannel`, `Mcu_InitClock`, `Gpt_StartTimer`, `Can_Write`, ...), takes a `const` configuration struct, reports development errors to Det with the official module IDs, and reaches registers only through base pointers (`bsw/mcal/reg/Mcal_Reg.h`). The firmware binds those pointers to the real addresses (`platform/stm32f103/Mcal_RegMap.c`) and the host tests bind them to RAM (`test/host/stubs/Mcal_FakeRegs.c`), so each driver is unit-tested on a PC.

In production the MCAL is supplied by the chip vendor (NXP RTD, Infineon MCAL, ST MCAL for SPC5/Stellar) and configured with their tools; no vendor ships one for the F103, so writing it by hand is honest and shows both register-level skill and AUTOSAR layering. CV wording is "AUTOSAR-style MCAL, hand-written at register level", never "AUTOSAR-compliant": it implements a subset of the APIs, has no ARXML configuration and was not conformance-tested.

Guided exercises (the learner writes the code, Claude reviews):

| # | Module | Guide | Host test (CMake option, OFF until done) |
|---|---|---|---|
| 1 | Port, Dio | [ex01-port-dio.vi.md](exercises/ex01-port-dio.vi.md) | `test_port_dio` (`MCAL_PORT_DIO_EXERCISE_TESTS`) |
| 2 | Mcu, Gpt | planned | planned |
| 3 | Can (bxCAN) | [ex03-can-bxcan.vi.md](exercises/ex03-can-bxcan.vi.md) | `test_can_hw` (`CAN_HW_EXERCISE_TESTS`) |
| 4 | Adc (optional) | planned | planned |

### Scope decision (2026-10-09): BSW only, no RTE/ASW
The project implements only the **BSW communication stack**. RTE and application SWCs are dropped on purpose: in industry they are produced by tool chains (Vector DaVinci, EB tresos, ETAS ISOLAR) that a personal project cannot access, so a hand-written "RTE" invites the question "how did you do that without the tools?". A hand-written subset of the Can/CanIf/PduR/Com SWS APIs, configured by a small DBC-to-C generator, is honest, explainable line by line, and is exactly what BSW integration/development roles work on daily.

### Option A (chosen): "AUTOSAR-like BSW CAN communication stack on STM32 (gateway ECU), DBC-driven, verified with log replay"

One-line pitch for the CV:
> Hand-wrote an AUTOSAR-style MCAL for STM32F103 at register level (Mcu, Port, Dio, Gpt, Can/bxCAN) with SWS API names, const configuration and Det error reporting, unit-tested on host against fake registers; on top of it, an AUTOSAR Classic–inspired CAN communication stack (CanIf, PduR with PDU gateway, Com with signal gateway, deadline monitoring and periodic TX) configured from DBC by a Python generator; verified by replaying real-vehicle CAN logs and decoding the output in SavvyCAN; MISRA C:2012 oriented, CI on GitHub Actions.

Architecture:

```
 PC side                                   STM32F103 (Blue Pill)
 ┌───────────────────────┐   UART 1M   ┌─────────────────────────────────────────┐
 │ replay.py             │────────────▶│ CddUartGw  (log frames in / out)        │
 │  reads .csv/.asc/.log │◀────────────│        │                                │
 │  real timestamps      │             │        ▼                                │
 └───────────────────────┘             │ Can    (MCAL, bxCAN, loopback/normal)   │
                                       │ Mcu/Port/Dio/Gpt (MCAL, registers)      │
 ┌───────────────────────┐             │ CanIf  (HRH/HTH mapping, RX indication) │
 │ SavvyCAN              │◀── capture ─│ PduR   (routing table, PDU gateway)     │
 │  DBC decode / graphs  │   (UART or  │ Com    (signal pack/unpack, signal      │
 │  verify ECU outputs   │   CANable)  │         gateway, deadline monitoring,   │
 └───────────────────────┘             │         periodic TX)                    │
 ┌───────────────────────┐             │ ComTest (thin hook calling the public   │
 │ dbc2c.py (codegen)    │── *.c/h ──▶ │          Com_ReceiveSignal/SendSignal)  │
 └───────────────────────┘             │ Os     (1 ms SysTick cooperative tasks) │
                                       │ Det    (dev error tracer → UART)        │
                                       └─────────────────────────────────────────┘
```

Data flow: a real-car log is replayed with original timing → frames enter the STM32 → `Can` → `CanIf` → `PduR`. PduR either routes a whole PDU to a TX PDU (PDU gateway, e.g. ID translation) or hands it to `Com`. `Com` unpacks signals, applies the **signal gateway** (copy selected input signals into output PDUs defined in **your own output DBC**) and transmits output PDUs periodically → captured in SavvyCAN and decoded with that output DBC. Stop the replay → Com deadline monitoring fires → substitute values + Det report visible in the log. That timeout demo is a strong interview talking point. No application logic is needed: a gateway ECU is a legitimate BSW-only product.

Why this scope:
- Matches the constraint (log files, no car) while still being real embedded work on your hardware.
- Every line is yours and explainable; no "fake" RTE pretending to be tool output.
- The DBC-to-C generator mirrors what configuration tools do (`Com_Cfg.c`, `PduR_Cfg.c`), so you can explain generated code in interviews.
- Scales: optional add-ons are E2E (CRC8 SAE-J1850 + alive counter) in Com, UDS ReadDataByIdentifier (0x22) via a minimal CanTp/Dcm, a second Blue Pill as a real bus partner.

Repository: `github.com/hi3rdt/can-log-analyzer`. Layout:

```
can-log-analyzer/
├── bsw/
│   ├── include/         Std_Types.h, ComStack_Types.h, Platform_Types.h
│   ├── mcal/reg/        Mcal_Reg.h (register structs + base pointers)
│   ├── mcal/port, dio/  Port.c, Dio.c                     (GPIO)
│   ├── mcal/mcu, gpt/   Mcu.c, Gpt.c                      (clock tree, timer)
│   ├── mcal/can/        Can.c, Can_Hw.c, Can.h            (bxCAN register-level)
│   ├── ecual/canif/     CanIf.c, CanIf_Cfg.c
│   ├── services/pdur/   PduR.c, PduR_Cfg.c
│   ├── services/com/    Com.c, Com_Cfg.c (generated)
│   ├── services/det/    Det.c
│   └── os/              Os.c (SysTick scheduler)
├── cdd/uartgw/          CddUartGw.c (frame bridge to PC)
├── platform/stm32f103/  startup, linker script, Mcal_RegMap.c (real addresses)
├── app/                 main.c + ComTest hook (no RTE, no SWC)
├── config/              input.dbc, output.dbc, gateway.yaml
├── tools/               dbc2c.py, replay.py, verify.py (cantools)
├── test/host/           host unit tests for Com, PduR, CanIf, timeouts
├── docs/                roadmap, architecture.md, SavvyCAN screenshots
└── CMakeLists.txt       arm-none-eabi firmware + host test targets
```

Milestones (one PR each):
1. Repo skeleton: CMake (host tests + arm-none-eabi firmware), STM32F103 startup/linker script, `Std_Types.h`/`ComStack_Types.h`, Det, CI.
2. Com path on host: `CanIf` → `PduR` → `Com` with `dbc2c.py` generating `Com_Cfg.c` / `PduR_Cfg.c` / `CanIf_Cfg.c`, host unit tests.
3. MCAL base: register layer, Port + Dio (exercise 1), Mcu + Gpt (exercise 2); `main.c` blinks the LED through `Port_Init` / `Dio_FlipChannel` on a Gpt tick.
4. `Can` MCAL driver for bxCAN (exercise 3, loopback first) + `CddUartGw`; frames visible on PC.
5. Os scheduler, Com periodic TX + deadline monitoring, signal gateway; replay a real log, outputs decoded in SavvyCAN.
6. Bus-off recovery, optional Adc, MISRA check (cppcheck `--addon=misra`), README with diagrams, results, demo GIF.

### Option B (not chosen as main project): "CAN log analyzer & signal reverse-engineering tool"
C++/Python tool that loads SavvyCAN/candump logs + DBC, decodes signals, detects anomalies (missing periodic frames, counter jumps, bad checksums, bus load), and exports reports. Kept only as a side tool inside Option A (`tools/verify.py`).

---

## 3. Reference code — Com signal unpack (Intel byte order)

```c
/* Com_Signal.c - generic Intel (little-endian) signal extraction.
 * MISRA C:2012 notes: fixed-width types, no dynamic memory, single exit point. */
#include <stdint.h>

typedef struct
{
    uint8_t  startBit;   /* DBC start bit (LSB position for Intel) */
    uint8_t  bitLength;  /* 1..32 */
    uint8_t  isSigned;   /* 0 = unsigned, 1 = two's complement */
} Com_SignalCfgType;

/* Returns the raw value; caller (Com user) applies factor/offset. */
uint32_t Com_UnpackIntel(const uint8_t *pdu, const Com_SignalCfgType *cfg)
{
    uint64_t frame = 0U;
    uint32_t raw;
    uint8_t  i;

    /* Assemble the 8-byte payload into a 64-bit word, byte 0 = LSB */
    for (i = 0U; i < 8U; i++)
    {
        frame |= ((uint64_t)pdu[i]) << (8U * i);
    }

    raw = (uint32_t)((frame >> cfg->startBit) &
                     ((cfg->bitLength == 32U) ? 0xFFFFFFFFULL
                                              : ((1ULL << cfg->bitLength) - 1ULL)));

    /* Sign-extend if required (stored as uint32_t, reinterpret at the RTE) */
    if ((cfg->isSigned != 0U) && (cfg->bitLength < 32U) &&
        ((raw & (1UL << (cfg->bitLength - 1U))) != 0U))
    {
        raw |= ~((1UL << cfg->bitLength) - 1UL);
    }

    return raw;
}
```

Edge cases to test: `bitLength` = 1 and 32, signal crossing a byte boundary, DLC < 8 (caller must zero-pad), signed min value, and Motorola signals (separate function — DBC start bit is the MSB in "sawtooth" numbering).

---

## 4. Shopping list (optional, cheap)
- SN65HVD230 transceiver module ×2 (~1 USD each) — needed for any real bus.
- USB-UART adapter (CP2102/CH340) — needed for Option A.
- Second Blue Pill — acts as a "gateway" or second ECU; lets you leave loopback mode.
- CANable / candleLight-compatible USB-CAN (~15–25 USD) — lets SavvyCAN sniff the real bus directly (verify the exact device support for your OS in SavvyCAN's connection dialog before buying).
- 120 Ω termination at both bus ends.
