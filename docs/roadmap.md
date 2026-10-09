# Learning Roadmap & CV Project — AUTOSAR-style BSW/ASW + CAN on STM32F103

Owner: hi3rdt · Date: 2026-10-09
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

### Phase 3 — Bare-metal bxCAN driver on STM32 (week 3–5)
- Clock tree (HSE 8 MHz → 72 MHz, APB1 = 36 MHz), bit timing for 500 kbit/s: prescaler 4, BS1 = 15, BS2 = 2, SJW = 1 → 36 MHz / 4 / 18 TQ = 500 kbit/s, sample point 16/18 = 88.9%.
- Init/sleep/normal mode transitions (INRQ/INAK, SLEEP/SLAK), loopback mode for self-test.
- 3 TX mailboxes, 2 RX FIFOs (3 deep each), 14 filter banks: ID-list vs. mask mode.
- Interrupt-driven RX (USB_LP_CAN1_RX0 IRQ), TX-complete, error/bus-off handling (ABOM).
- UART logging of every frame in **GVRET-like / SLCAN text** so SavvyCAN or a Python script can capture it.
- Deliverable: STM32 in loopback receives its own frames; SavvyCAN shows them.

### Phase 4 — AUTOSAR concepts mapped onto your code (week 4–6, overlaps)
- Layered architecture: MCAL → ECU Abstraction → Services → RTE → SWC.
- Communication stack path: `Can` → `CanIf` → `PduR` → `Com` → `RTE` → SWC (and back for TX).
- Concepts to actually implement (simplified): PDU, I-PDU vs. L-PDU, signal/PDU configuration tables, Com TX modes (periodic / direct / mixed), deadline monitoring (timeout → substitute value), update bits.
- Read-only theory (no implementation needed for CV): EcuM/BswM, Det, NvM/MemIf, Dcm/UDS basics, E2E protection profiles (CRC + alive counter).
- Reference: AUTOSAR Classic SWS documents for Can, CanIf, PduR, Com (free on autosar.org) — read only API + config sections.

### Phase 5 — CV project build (week 6–10)
See section 2. Final week: README, architecture diagram, demo video/GIF from SavvyCAN, MISRA report.

---

## 2. CV project options

### Option A (recommended): "Mini AUTOSAR-like CAN ECU on STM32 with DBC-driven Com stack and log replay"

One-line pitch for the CV:
> Designed a layered (AUTOSAR Classic–inspired) CAN ECU firmware on STM32F103: bare-metal bxCAN driver, CanIf/PduR/Com/RTE layers generated from a DBC, application SWCs, log-replay HIL setup verified in SavvyCAN; MISRA C:2012 checked, unit-tested on host.

Architecture:

```
 PC side                                   STM32F103 (Blue Pill)
 ┌───────────────────────┐   UART 1M   ┌─────────────────────────────────────────┐
 │ replay.py             │────────────▶│ CddUartGw  (log frames in / out)        │
 │  reads .csv/.asc/.log │◀────────────│        │                                │
 │  real timestamps      │             │        ▼                                │
 └───────────────────────┘             │ Can (MCAL, bxCAN, loopback or normal)   │
 ┌───────────────────────┐             │ CanIf  (HRH/HTH mapping, RX indication) │
 │ SavvyCAN              │◀── capture ─│ PduR   (routing table)                  │
 │  DBC decode / graphs  │   (UART or  │ Com    (signal pack/unpack, timeouts,   │
 │  verify ECU outputs   │   CANable)  │         periodic TX)                    │
 └───────────────────────┘             │ Rte    (generated Rte_Read/Rte_Write)   │
 ┌───────────────────────┐             │ SWC: SpeedMonitor, LightCtrl, DoorLogic │
 │ dbc2c.py (codegen)    │── *.c/h ──▶ │ OS: 1 ms SysTick cooperative scheduler  │
 └───────────────────────┘             │      (1/10/100 ms tasks)                │
                                       │ Det (dev error tracer → UART)           │
                                       └─────────────────────────────────────────┘
```

Data flow: a real-car log is replayed with original timing → frames enter the STM32 → go through the full stack → SWCs compute outputs (e.g. "overspeed warning", "lights on when dark & speed > 0", "door unlock on crash/stop") → output PDUs are transmitted periodically by Com → captured in SavvyCAN and decoded with **your own output DBC**. Stop the replay → Com deadline monitoring fires → substitute values + Det report visible in the log. That timeout demo is a strong interview talking point.

Why this one:
- Matches the constraint (log files, no car) while still being real embedded work on your hardware.
- Shows exactly the BSW vs. ASW split recruiters for Bosch/Continental/FPT/LG VS look for.
- Code generation from DBC mirrors how real AUTOSAR configuration tools (DaVinci, EB tresos) produce `Com_Cfg.c`.
- Scales: optional add-ons are E2E (CRC8 SAE-J1850 + alive counter), UDS ReadDataByIdentifier (0x22) on a diagnostic ID, a second Blue Pill as a real bus partner.

Suggested repository layout:

```
mini-autosar-can-ecu/
├── bsw/
│   ├── mcal/can/        Can.c, Can.h, Can_Cfg.h           (bxCAN register-level)
│   ├── ecual/canif/     CanIf.c, CanIf_Cfg.c
│   ├── services/pdur/   PduR.c, PduR_Cfg.c
│   ├── services/com/    Com.c, Com_Cfg.c (generated)
│   ├── services/det/    Det.c
│   └── os/              Os.c (SysTick scheduler)
├── rte/                 Rte.c, Rte_<Swc>.h (generated)
├── asw/                 SpeedMonitor.c, LightCtrl.c, DoorLogic.c
├── cdd/uartgw/          CddUartGw.c (frame bridge to PC)
├── config/              input.dbc, output.dbc, ecu_config.yaml
├── tools/               dbc2c.py, replay.py, verify.py (cantools)
├── test/host/           Unity tests for Com pack/unpack, PduR, timeouts
├── docs/                architecture.md, diagrams, SavvyCAN screenshots
└── CMakeLists.txt       arm-none-eabi + host test targets
```

Milestones:
1. Host-tested `Com` pack/unpack + `dbc2c.py` generator (Phase 2 output reused).
2. `Can` driver in loopback + `CddUartGw`; frames visible on PC.
3. `CanIf` → `PduR` → `Com` → `Rte` path; one SWC reading one signal.
4. Replay a real log, 3 SWCs, output PDUs decoded in SavvyCAN.
5. Deadline monitoring, Det, bus-off recovery; MISRA check (cppcheck `--addon=misra`), CI on GitHub Actions (host tests + cross build).
6. README with diagrams, results, demo GIF.

### Option B (lighter, PC-only): "CAN log analyzer & signal reverse-engineering tool"
C++/Python tool that loads SavvyCAN/candump logs + DBC, decodes signals, detects anomalies (missing periodic frames, counter jumps, bad checksums, bus load), and exports reports. Faster to finish, but shows little embedded/BSW skill — use it only as a side tool inside Option A (`tools/verify.py`), not as the main CV project.

**Recommendation: Option A**, with Option B's analyzer folded in as the verification tool.

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

/* Returns the raw value; caller applies factor/offset in the RTE/SWC layer. */
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
