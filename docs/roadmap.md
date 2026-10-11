# Learning Roadmap & CV Project — AUTOSAR-style MCAL + DBC with SavvyCAN on STM32F103

Owner: hi3rdt · Date: 2026-10-09 · Updated: 2026-10-11 (scope: MCAL + DBC with SavvyCAN)
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

### Phase 2 — Writing a DBC & the signal math behind it (week 2–3, DBC track)
- Write your own DBC for a real log in SavvyCAN (exercise D1): `BO_` / `SG_` syntax, start bit and length, `@1` vs `@0`, `+`/`-`, factor/offset, `VAL_` tables, `CM_` comments, cycle-time attributes. Check it with cantools against the log.
- Intel (little-endian) vs. Motorola (big-endian) bit numbering — the #1 source of decode bugs.
- Physical = raw × factor + offset; signed signals; min/max; multiplexed signals.
- Write a host-side C library `CanSig_Unpack/Pack` + unit tests (Unity or CppUTest) on PC, validate against `cantools` (Python) output for the same log.

### Phase 3 — Register-level MCAL on STM32: Port/Dio, Mcu/Gpt, then bxCAN (week 3–5)
- GPIO first (exercise 1): CRL/CRH 4-bit CNF|MODE per pin, IDR/ODR, atomic BSRR, RCC clock gating. Then Mcu (HSE → PLL 72 MHz, flash wait states) and Gpt (TIM2 as a 1 ms tick).
- Clock tree (HSE 8 MHz → 72 MHz, APB1 = 36 MHz), bit timing for 500 kbit/s: prescaler 4, BS1 = 15, BS2 = 2, SJW = 1 → 36 MHz / 4 / 18 TQ = 500 kbit/s, sample point 16/18 = 88.9%.
- Init/sleep/normal mode transitions (INRQ/INAK, SLEEP/SLAK), loopback mode for self-test.
- 3 TX mailboxes, 2 RX FIFOs (3 deep each), 14 filter banks: ID-list vs. mask mode.
- Interrupt-driven RX (USB_LP_CAN1_RX0 IRQ), TX-complete, error/bus-off handling (ABOM).
- Uart driver streaming every frame in the **LAWICEL/SLCAN text protocol**, which SavvyCAN reads directly through a LAWICEL serial connection.
- Deliverable: STM32 in loopback receives its own frames; SavvyCAN shows them live and decodes them with your DBC.

### Phase 4 — AUTOSAR concepts mapped onto your code (week 4–6, overlaps)
- Layered architecture: MCAL → ECU Abstraction → Services → RTE → SWC. Know all layers, **implement only the MCAL** (section 2). The upper layers are studied, not built: the Can driver stops at the `CanIf_Cbk.h` callbacks, which a small test harness in `app/` implements.
- Communication stack path in a real ECU: `Can` → `CanIf` → `PduR` → `Com` → RTE → SWC. In this project the boundary is the Can driver's callbacks (`CanIf_Cbk.h`).
- Concepts to understand for interviews (not implemented): PDU, I-PDU vs. L-PDU, signal/PDU configuration tables, Com TX modes (periodic / direct / mixed), deadline monitoring (timeout → substitute value), update bits, PduR PDU gateway, Com signal gateway. Milestone 2 implemented a simplified version of these; it is in the git history (removed 2026-10-11).
- Read-only theory (no implementation needed for CV): RTE and SWC/runnables, EcuM/BswM, NvM/MemIf, Dcm/UDS basics, E2E protection profiles (CRC + alive counter).
- Reference: AUTOSAR Classic SWS documents for Can, CanIf, PduR, Com (free on autosar.org) — read only API + config sections.

### Phase 5 — CV project build (week 6–10)
See section 2. Final week: README, architecture diagram, demo video/GIF of SavvyCAN decoding the Blue Pill's frames with your DBC, MISRA report.

---

## 2. CV project

### Current scope (2026-10-11): MCAL + DBC with SavvyCAN
The project has two tracks and nothing else:

1. **MCAL track.** A hand-written **AUTOSAR-style MCAL** on the STM32F103 registers: Port + Dio (GPIO), Mcu (clock tree) + Gpt (hardware timer), Can (bxCAN), a Uart driver that streams frames to SavvyCAN in the LAWICEL/SLCAN text protocol, and optionally Adc. Every driver uses the SWS API names (`Port_Init`, `Dio_WriteChannel`, `Mcu_InitClock`, `Gpt_StartTimer`, `Can_Write`, ...), takes a `const` configuration struct, reports development errors to Det with the official module IDs, and reaches registers only through base pointers (`bsw/mcal/reg/Mcal_Reg.h`). The firmware binds those pointers to the real addresses (`platform/stm32f103/Mcal_RegMap.c`) and the host tests bind them to RAM (`test/host/stubs/Mcal_FakeRegs.c`), so each driver is unit-tested on a PC.
2. **DBC track.** Reverse-engineer real CAN logs in SavvyCAN and write the result as a DBC file: message periods, alive counters, CRC-8 SAE J1850 and sum checksums, 16-bit physical values, byte order and sign. The DBC is checked against the log with cantools, and later used in SavvyCAN to decode the frames the Blue Pill sends through the Can and Uart drivers.

**Out of scope:** new BSW services (CanIf, PduR, Com, CanTp, Dcm, NvM, ...), RTE and ASW/SWCs. In industry these are configured and generated with tools (Vector DaVinci, EB tresos, ETAS ISOLAR) that a personal project does not have. The MCAL is different: vendors ship it as plain C drivers, and no vendor ships one for the F103, so writing it by hand is both credible and the best way to show register-level skill.

**The CanIf/PduR/Com code of milestone 2 was removed (2026-10-11)**, together with its generated configuration and the `dbc2c.py` generator; it remains in the git history. The Can driver's only upward dependency is `bsw/include/CanIf_Cbk.h` (`CanIf_RxIndication`, `CanIf_TxConfirmation`), implemented by the test harness in `app/`.

CV wording is "AUTOSAR-style MCAL, hand-written at register level", never "AUTOSAR-compliant": the project implements a subset of the APIs, has no ARXML configuration and was not conformance-tested.

One-line pitch for the CV:
> Hand-wrote an AUTOSAR-style MCAL for STM32F103 at register level (Mcu, Port, Dio, Gpt, Can/bxCAN, Uart) with SWS API names, const configuration and Det error reporting, unit-tested on host against fake registers with CI on GitHub Actions; reverse-engineered real-vehicle CAN logs in SavvyCAN into a DBC (alive counters, CRC-8 SAE J1850 and sum checksums, verified with cantools) and used it to decode the driver's live traffic streamed to SavvyCAN; MISRA C:2012 oriented.

Architecture:

```
 PC side                                   STM32F103 (Blue Pill)
 ┌───────────────────────┐  UART (SLCAN) ┌─────────────────────────────────────────┐
 │ SavvyCAN              │◀──────────────│ app/main.c  (test harness, no RTE/SWC)  │
 │  LAWICEL connection   │               │        │                                │
 │  your DBC decodes the │               │        ▼                                │
 │  frames live          │               │ MCAL                                    │
 └───────────────────────┘               │  Can   (bxCAN, loopback/normal)         │
 ┌───────────────────────┐               │  Uart  (SLCAN frame stream)             │
 │ dbc/*.dbc (yours)     │               │  Port / Dio  (pins, LED)                │
 │  checked by cantools  │               │  Mcu / Gpt   (72 MHz clock, 1 ms tick)  │
 │  against real logs    │               │  Adc   (optional)                       │
 └───────────────────────┘               │ Det    (development error tracer)       │
                                         └─────────────────────────────────────────┘
```

Repository layout (target):

```
can-log-analyzer/
├── bsw/
│   ├── include/         Std_Types.h, Can_GeneralTypes.h, CanIf_Cbk.h (callbacks, implemented in app/)
│   ├── mcal/reg/        Mcal_Reg.h (register structs + base pointers)
│   ├── mcal/port, dio/  Port.c, Dio.c                     (GPIO)
│   ├── mcal/mcu, gpt/   Mcu.c, Gpt.c                      (clock tree, timer)
│   ├── mcal/can/        Can.c, Can_Hw.c, Can.h            (bxCAN register-level)
│   ├── mcal/uart/       Uart.c                            (USART1, SLCAN stream)
│   └── services/det/    Det.c
├── dbc/                 DBC files you write from the logs (DBC track)
├── platform/stm32f103/  startup, linker script, Mcal_RegMap.c (real addresses)
├── app/                 main.c test harness (no RTE, no SWC)
├── samples/             demo drive log (GVRET CSV + candump)
├── tools/               check_dbc.py, demo log generator (cantools)
├── test/host/           host unit tests, fake MCAL registers
└── docs/                roadmap, SavvyCAN guides, exercises
```

Milestones:
1. Repo skeleton: CMake (host tests + arm-none-eabi firmware), startup/linker script, Det, CI. Done.
2. CanIf → PduR → Com with a DBC-to-C generator. Done, then removed when the scope was narrowed (in git history).
3. MCAL base: register layer, Port + Dio (exercise M1), Mcu + Gpt (exercise M2); `main.c` blinks the LED through `Port_Init` / `Dio_FlipChannel` on a Gpt tick.
4. Can driver for bxCAN (exercise M3, loopback first) + Uart SLCAN stream (exercise M4); SavvyCAN shows the Blue Pill's frames live over a LAWICEL connection.
5. DBC track: a DBC for `GVRET_Log.csv` (exercise D1) and for `0x2F0` in the demo log (SavvyCAN guide, exercise 4), each checked with cantools; the Blue Pill transmits frames described by your DBC and SavvyCAN decodes them.
6. Bus-off handling, optional Adc, MISRA check (cppcheck `--addon=misra`), README with diagrams, demo GIF.

Guided exercises (the learner writes the code and the DBC, Claude reviews):

| # | Track | Topic | Guide | Check |
|---|---|---|---|---|
| M1 | MCAL | Port, Dio | [ex01-port-dio.vi.md](exercises/ex01-port-dio.vi.md) | `test_port_dio` (`MCAL_PORT_DIO_EXERCISE_TESTS`) |
| M2 | MCAL | Mcu, Gpt | planned | planned |
| M3 | MCAL | Can (bxCAN) | [ex03-can-bxcan.vi.md](exercises/ex03-can-bxcan.vi.md) | `test_can_hw` (`CAN_HW_EXERCISE_TESTS`) |
| M4 | MCAL | Uart + SLCAN to SavvyCAN | planned | planned |
| M5 | MCAL | Adc (optional) | planned | planned |
| D1 | DBC | DBC for `GVRET_Log.csv` | [dbc01-gvret-log.vi.md](exercises/dbc01-gvret-log.vi.md) | cantools + SavvyCAN |

### Scope history
- 2026-10-09: Option A chosen (CAN gateway ECU on STM32, verified with log replay), narrowed to BSW only (Can, CanIf, PduR, Com, Det), no RTE/ASW. Milestones 1 and 2 were built under this scope.
- 2026-10-11: MCAL added below the stack ("MCAL + CAN stack").
- 2026-10-11: narrowed to MCAL + DBC with SavvyCAN, as described above. CanIf/PduR/Com, `gen/` and `dbc2c.py` removed from the repo at hi3rdt's request.
- Option B (a standalone log analyzer tool) was never the main project; its useful parts live on as the DBC track.

---

## 3. Reference code — signal unpack (Intel byte order), the math behind a DBC `SG_` line

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
- USB-UART adapter (CP2102/CH340) — needed for the Uart SLCAN link to SavvyCAN.
- Second Blue Pill — acts as a "gateway" or second ECU; lets you leave loopback mode.
- CANable / candleLight-compatible USB-CAN (~15–25 USD) — lets SavvyCAN sniff the real bus directly (verify the exact device support for your OS in SavvyCAN's connection dialog before buying).
- 120 Ω termination at both bus ends.
