#!/usr/bin/env python3
"""dbc2c.py - generate CanIf/PduR/Com configuration (C source) from a DBC file.

This plays the role of an AUTOSAR configuration tool (DaVinci, EB tresos) for
a single ECU ("ECU extract"):

  * messages transmitted by --node            -> Com TX I-PDUs + CanIf TX L-PDUs
  * messages with a signal received by --node -> CanIf RX L-PDUs + Com RX I-PDUs
  * routes.json "pdu_gateway" entries         -> PduR if-to-if routes
    (the TX side of a gateway route is a pure CanIf L-PDU, not a Com I-PDU)

Only the standard library is used so the tool runs anywhere. The DBC parser
supports BU_/BO_/SG_ lines, which is all the generator needs; multiplexed
signals, CAN FD (DLC > 8) and signals wider than 32 bits are rejected.

Usage:
    python3 tools/dbc2c.py --dbc config/gateway.dbc --routes config/routes.json --out gen
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

CAN_ID_EXTENDED_FLAG = 0x80000000

_BO_RE = re.compile(r"^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+)")
_SG_RE = re.compile(
    r"^SG_\s+(\w+)\s*(\S+)?\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*"
    r"\(\s*([^,]+)\s*,\s*([^)]+)\)\s*\[\s*([^|]*)\|([^\]]*)\]\s*\"([^\"]*)\"\s*(.*)$"
)


class DbcError(Exception):
    """Raised for DBC content the generator cannot handle."""


@dataclass
class Signal:
    name: str
    start_bit: int          # DBC start bit: LSB (Intel) or MSB (Motorola)
    length: int
    little_endian: bool
    is_signed: bool
    factor: float
    offset: float
    minimum: float
    maximum: float
    unit: str
    receivers: list[str]

    def bit_positions(self) -> list[int]:
        """Absolute DBC bit numbers covered by the signal, MSB..LSB or LSB..MSB."""
        positions = []
        pos = self.start_bit
        for _ in range(self.length):
            positions.append(pos)
            if self.little_endian:
                pos += 1
            elif pos % 8 == 0:
                pos += 15
            else:
                pos -= 1
        return positions

    @property
    def c_type(self) -> str:
        if self.length == 1 and not self.is_signed:
            return "COM_BOOLEAN"
        for width in (8, 16, 32):
            if self.length <= width:
                return f"COM_{'SINT' if self.is_signed else 'UINT'}{width}"
        raise DbcError(f"signal {self.name}: {self.length} bits > 32 not supported")

    def raw_init_value(self) -> int:
        """Raw value for physical 0 clamped to [min, max], as two's complement bits."""
        physical = 0.0
        if self.minimum < self.maximum:
            physical = min(max(physical, self.minimum), self.maximum)
        raw = int(round((physical - self.offset) / self.factor))
        return raw & ((1 << self.length) - 1)


@dataclass
class Message:
    can_id: int             # Includes CAN_ID_EXTENDED_FLAG for 29-bit IDs
    name: str
    dlc: int
    transmitter: str
    signals: list[Signal] = field(default_factory=list)

    @property
    def is_extended(self) -> bool:
        return bool(self.can_id & CAN_ID_EXTENDED_FLAG)


def parse_dbc(text: str) -> list[Message]:
    messages: list[Message] = []
    current: Message | None = None
    for lineno, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if line.startswith("BO_ "):
            m = _BO_RE.match(line)
            if not m:
                raise DbcError(f"line {lineno}: cannot parse message: {line}")
            can_id, name, dlc, tx = int(m.group(1)), m.group(2), int(m.group(3)), m.group(4)
            if dlc > 8:
                raise DbcError(f"line {lineno}: {name}: DLC {dlc} > 8 (CAN FD) not supported")
            if not can_id & CAN_ID_EXTENDED_FLAG and can_id > 0x7FF:
                raise DbcError(f"line {lineno}: {name}: standard ID 0x{can_id:X} > 0x7FF")
            current = Message(can_id, name, dlc, tx)
            messages.append(current)
        elif line.startswith("SG_ "):
            if current is None:
                raise DbcError(f"line {lineno}: signal outside a message")
            m = _SG_RE.match(line)
            if not m:
                raise DbcError(f"line {lineno}: cannot parse signal: {line}")
            if m.group(2):
                raise DbcError(f"line {lineno}: multiplexed signal {m.group(1)} not supported")
            receivers = [r.strip() for r in m.group(12).split(",") if r.strip()]
            current.signals.append(Signal(
                name=m.group(1),
                start_bit=int(m.group(3)),
                length=int(m.group(4)),
                little_endian=m.group(5) == "1",
                is_signed=m.group(6) == "-",
                factor=float(m.group(7)),
                offset=float(m.group(8)),
                minimum=float(m.group(9) or 0),
                maximum=float(m.group(10) or 0),
                unit=m.group(11),
                receivers=receivers,
            ))
        elif line and not raw_line.startswith((" ", "\t")):
            current = None
    return messages


def validate(messages: list[Message]) -> None:
    seen_ids: dict[int, str] = {}
    for msg in messages:
        if msg.can_id in seen_ids:
            raise DbcError(f"{msg.name}: duplicate CAN ID with {seen_ids[msg.can_id]}")
        seen_ids[msg.can_id] = msg.name
        used: dict[int, str] = {}
        for sig in msg.signals:
            if sig.length < 1 or sig.factor == 0:
                raise DbcError(f"{msg.name}.{sig.name}: invalid length or factor")
            _ = sig.c_type
            for bit in sig.bit_positions():
                if bit < 0 or bit >= msg.dlc * 8:
                    raise DbcError(f"{msg.name}.{sig.name}: bit {bit} outside DLC {msg.dlc}")
                if bit in used:
                    raise DbcError(f"{msg.name}.{sig.name}: overlaps {used[bit]} at bit {bit}")
                used[bit] = sig.name


@dataclass
class EcuConfig:
    """Resolved ID assignment for one ECU, shared by all generators."""
    com_ipdus: list[tuple[Message, str]]        # (message, "RX" | "TX")
    com_signals: list[tuple[Signal, int]]       # (signal, com ipdu index)
    canif_rx: list[Message]                     # sorted by CAN ID
    canif_tx: list[Message]                     # Com TX first, then gateway targets
    pdur_rx_dests: list[list[tuple[str, int]]]  # per canif_rx entry: (module, pdu id)
    pdur_com_tx: list[int]                      # Com TX route k -> CanIf TX id
    com_tx_route: dict[int, int]                # Com ipdu index -> PduR Com TX id
    pdur_txconf: list[int | None]               # CanIf TX id -> Com ipdu index or None


def build_config(messages: list[Message], node: str, gateway: list[dict]) -> EcuConfig:
    by_name = {m.name: m for m in messages}
    gw_pairs = []
    for route in gateway:
        try:
            src, dst = by_name[route["rx"]], by_name[route["tx"]]
        except KeyError as exc:
            raise DbcError(f"routes.json: unknown message {exc}") from None
        if dst.transmitter != node:
            raise DbcError(f"gateway target {dst.name} is not transmitted by {node}")
        if dst.dlc < src.dlc:
            raise DbcError(f"gateway {src.name} -> {dst.name}: target DLC smaller than source")
        gw_pairs.append((src, dst))
    gw_targets = {dst.name for _, dst in gw_pairs}
    gw_sources = {src.name for src, _ in gw_pairs}

    com_rx = [m for m in messages if any(node in s.receivers for s in m.signals)]
    com_tx = [m for m in messages if m.transmitter == node and m.name not in gw_targets]
    com_rx.sort(key=lambda m: m.can_id)
    com_tx.sort(key=lambda m: m.can_id)

    com_ipdus = [(m, "RX") for m in com_rx] + [(m, "TX") for m in com_tx]
    if not com_ipdus:
        raise DbcError(f"node {node} neither transmits nor receives any signal")
    com_index = {m.name: i for i, (m, _) in enumerate(com_ipdus)}
    com_signals = []
    for idx, (msg, direction) in enumerate(com_ipdus):
        for sig in msg.signals:
            # RX I-PDUs expose only signals this node receives
            if direction == "TX" or node in sig.receivers:
                com_signals.append((sig, idx))

    canif_rx = sorted({m.name: m for m in com_rx + [s for s, _ in gw_pairs]}.values(),
                      key=lambda m: m.can_id)
    canif_tx = com_tx + [dst for _, dst in gw_pairs]
    canif_tx_index = {m.name: i for i, m in enumerate(canif_tx)}

    pdur_rx_dests = []
    for msg in canif_rx:
        dests = []
        if msg.name in com_index and com_ipdus[com_index[msg.name]][1] == "RX":
            dests.append(("PDUR_DEST_COM", com_index[msg.name]))
        if msg.name in gw_sources:
            for src, dst in gw_pairs:
                if src.name == msg.name:
                    dests.append(("PDUR_DEST_CANIF", canif_tx_index[dst.name]))
        pdur_rx_dests.append(dests)

    pdur_com_tx = [canif_tx_index[m.name] for m in com_tx]
    com_tx_route = {com_index[m.name]: k for k, m in enumerate(com_tx)}
    pdur_txconf = [com_index[m.name] if m.name not in gw_targets else None for m in canif_tx]

    return EcuConfig(com_ipdus, com_signals, canif_rx, canif_tx,
                     pdur_rx_dests, pdur_com_tx, com_tx_route, pdur_txconf)


HEADER = """/*
 * GENERATED FILE - DO NOT EDIT.
 * Source: {dbc} (node {node}), {routes}
 * Generator: tools/dbc2c.py
 */
"""


def _fmt_float(value: float) -> str:
    return f"{value:g}"


def gen_com(cfg: EcuConfig, hdr: str) -> tuple[str, str]:
    h = [hdr, "#ifndef COM_CFG_H", "#define COM_CFG_H", "", '#include "Com_Types.h"', ""]
    h.append("/* Com I-PDU handles */")
    for i, (msg, direction) in enumerate(cfg.com_ipdus):
        h.append(f"#define ComConf_ComIPdu_{msg.name:<28} {i}U  /* {direction} 0x{msg.can_id & 0x1FFFFFFF:X} */")
    h.append("")
    h.append("/* Com signal handles: raw value; physical = raw * factor + offset */")
    for i, (sig, _) in enumerate(cfg.com_signals):
        unit = f" [{sig.unit}]" if sig.unit else ""
        h.append(f"#define ComConf_ComSignal_{sig.name:<26} {i}U  "
                 f"/* factor {_fmt_float(sig.factor)}, offset {_fmt_float(sig.offset)}{unit} */")
    h += ["", f"#define COM_NUM_IPDUS     {len(cfg.com_ipdus)}U",
          f"#define COM_NUM_SIGNALS   {len(cfg.com_signals)}U", "",
          "extern const Com_ConfigType Com_Config;", "", "#endif /* COM_CFG_H */", ""]

    c = [hdr, '#include "Com_Cfg.h"', ""]
    for msg, _ in cfg.com_ipdus:
        c.append(f"static uint8 Com_Buffer_{msg.name}[{max(msg.dlc, 1)}];")
    c += ["", "static const Com_IPduConfigType Com_IPdus[COM_NUM_IPDUS] =", "{"]
    for i, (msg, direction) in enumerate(cfg.com_ipdus):
        dir_c = "COM_RECEIVE" if direction == "RX" else "COM_SEND"
        route = cfg.com_tx_route.get(i, 0)
        c.append(f"    {{ {dir_c:<11}, {msg.dlc}U, Com_Buffer_{msg.name}, {route}U }},  /* {msg.name} */")
    c += ["};", "", "static const Com_SignalConfigType Com_Signals[COM_NUM_SIGNALS] =", "{"]
    c.append("    /* bitPos, size, endianness,          type,         ipdu, init */")
    for sig, ipdu in cfg.com_signals:
        endian = "COM_LITTLE_ENDIAN" if sig.little_endian else "COM_BIG_ENDIAN"
        c.append(f"    {{ {sig.start_bit:>2}U, {sig.length:>2}U, {endian:<17}, {sig.c_type:<12}, "
                 f"{ipdu}U, 0x{sig.raw_init_value():X}UL }},  /* {sig.name} */")
    c += ["};", "",
          "const Com_ConfigType Com_Config =", "{",
          "    Com_Signals, COM_NUM_SIGNALS,", "    Com_IPdus,   COM_NUM_IPDUS", "};", ""]
    return "\n".join(h), "\n".join(c)


def gen_canif(cfg: EcuConfig, hdr: str) -> tuple[str, str]:
    h = [hdr, "#ifndef CANIF_CFG_H", "#define CANIF_CFG_H", "", '#include "CanIf_Types.h"', "",
         "/* CanIf TX L-PDU handles */"]
    for i, msg in enumerate(cfg.canif_tx):
        h.append(f"#define CanIfConf_TxPdu_{msg.name:<28} {i}U")
    h += ["", f"#define CANIF_NUM_RX_PDUS {len(cfg.canif_rx)}U",
          f"#define CANIF_NUM_TX_PDUS {len(cfg.canif_tx)}U", "",
          "extern const CanIf_ConfigType CanIf_Config;", "", "#endif /* CANIF_CFG_H */", ""]

    def can_id_c(msg: Message) -> str:
        base = msg.can_id & 0x1FFFFFFF
        return f"(0x{base:08X}UL | CAN_ID_EXTENDED_FLAG)" if msg.is_extended else f"0x{base:03X}UL"

    c = [hdr, '#include "CanIf_Cfg.h"', "",
         "/* Sorted by CAN ID for binary search; upperPduId = PduR RX route */",
         "static const CanIf_RxPduConfigType CanIf_RxPdus[CANIF_NUM_RX_PDUS] =", "{"]
    for i, msg in enumerate(cfg.canif_rx):
        c.append(f"    {{ {can_id_c(msg)}, {msg.dlc}U, {i}U }},  /* {msg.name} */")
    c += ["};", "", "/* Single HTH 0: the bxCAN driver owns all three TX mailboxes */",
          "static const CanIf_TxPduConfigType CanIf_TxPdus[CANIF_NUM_TX_PDUS] =", "{"]
    for i, msg in enumerate(cfg.canif_tx):
        c.append(f"    {{ {can_id_c(msg)}, {msg.dlc}U, 0U, {i}U }},  /* {msg.name} */")
    c += ["};", "", "const CanIf_ConfigType CanIf_Config =", "{",
          "    CanIf_RxPdus, CANIF_NUM_RX_PDUS,", "    CanIf_TxPdus, CANIF_NUM_TX_PDUS", "};", ""]
    return "\n".join(h), "\n".join(c)


def gen_pdur(cfg: EcuConfig, hdr: str) -> tuple[str, str]:
    h = [hdr, "#ifndef PDUR_CFG_H", "#define PDUR_CFG_H", "", '#include "PduR_Types.h"', "",
         f"#define PDUR_NUM_RX_ROUTES      {len(cfg.canif_rx)}U",
         f"#define PDUR_NUM_COM_TX_ROUTES  {len(cfg.pdur_com_tx)}U",
         f"#define PDUR_NUM_TXCONF_ROUTES  {len(cfg.canif_tx)}U", "",
         "extern const PduR_PBConfigType PduR_Config;", "", "#endif /* PDUR_CFG_H */", ""]

    c = [hdr, '#include "PduR_Cfg.h"', ""]
    for msg, dests in zip(cfg.canif_rx, cfg.pdur_rx_dests):
        c.append(f"static const PduR_DestPduType PduR_Dests_{msg.name}[{len(dests)}] =")
        c.append("{")
        for module, pdu in dests:
            c.append(f"    {{ {module}, {pdu}U }},")
        c += ["};", ""]
    c += ["static const PduR_RxRouteType PduR_RxRoutes[PDUR_NUM_RX_ROUTES] =", "{"]
    for msg, dests in zip(cfg.canif_rx, cfg.pdur_rx_dests):
        c.append(f"    {{ PduR_Dests_{msg.name}, {len(dests)}U }},")
    c += ["};", ""]
    if cfg.pdur_com_tx:
        c += ["static const PduR_ComTxRouteType PduR_ComTxRoutes[PDUR_NUM_COM_TX_ROUTES] =", "{"]
        for canif_id in cfg.pdur_com_tx:
            c.append(f"    {{ {canif_id}U }},  /* {cfg.canif_tx[canif_id].name} */")
        c += ["};", ""]
    c += ["static const PduR_TxConfRouteType PduR_TxConfRoutes[PDUR_NUM_TXCONF_ROUTES] =", "{"]
    for msg, com_idx in zip(cfg.canif_tx, cfg.pdur_txconf):
        if com_idx is None:
            c.append(f"    {{ FALSE, 0U }},  /* {msg.name} (gateway) */")
        else:
            c.append(f"    {{ TRUE, {com_idx}U }},  /* {msg.name} */")
    com_tx_ref = "PduR_ComTxRoutes" if cfg.pdur_com_tx else "NULL_PTR"
    c += ["};", "", "const PduR_PBConfigType PduR_Config =", "{",
          "    PduR_RxRoutes,     PDUR_NUM_RX_ROUTES,",
          f"    {com_tx_ref}, PDUR_NUM_COM_TX_ROUTES,",
          "    PduR_TxConfRoutes, PDUR_NUM_TXCONF_ROUTES", "};", ""]
    return "\n".join(h), "\n".join(c)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dbc", required=True, type=Path)
    ap.add_argument("--routes", required=True, type=Path,
                    help='JSON: {"node": "GW", "pdu_gateway": [{"rx": "...", "tx": "..."}]}')
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args(argv)

    routes = json.loads(args.routes.read_text(encoding="utf-8"))
    node = routes["node"]
    try:
        messages = parse_dbc(args.dbc.read_text(encoding="utf-8", errors="replace"))
        validate(messages)
        cfg = build_config(messages, node, routes.get("pdu_gateway", []))
    except DbcError as exc:
        print(f"dbc2c: error: {exc}", file=sys.stderr)
        return 1

    hdr = HEADER.format(dbc=args.dbc.as_posix(), node=node, routes=args.routes.as_posix())
    args.out.mkdir(parents=True, exist_ok=True)
    for base, (h, c) in (("Com_Cfg", gen_com(cfg, hdr)),
                         ("CanIf_Cfg", gen_canif(cfg, hdr)),
                         ("PduR_Cfg", gen_pdur(cfg, hdr))):
        (args.out / f"{base}.h").write_text(h, encoding="utf-8")
        (args.out / f"{base}.c").write_text(c, encoding="utf-8")
    print(f"dbc2c: {len(cfg.com_ipdus)} Com I-PDUs, {len(cfg.com_signals)} signals, "
          f"{len(cfg.canif_rx)} CanIf RX, {len(cfg.canif_tx)} CanIf TX -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
