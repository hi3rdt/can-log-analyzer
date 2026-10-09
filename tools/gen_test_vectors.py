#!/usr/bin/env python3
"""gen_test_vectors.py - build C test vectors for Com with cantools as the oracle.

cantools is an independent DBC implementation, so comparing our C pack/unpack
against it catches bit-numbering mistakes (Motorola sawtooth, sign extension)
that hand-written expectations could share with the implementation.

    pip install cantools
    python3 tools/gen_test_vectors.py --dbc config/gateway.dbc --routes config/routes.json \
        --out test/host/test_vectors.h
"""
from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path

import cantools

FRAMES_PER_RX_MESSAGE = 32
FRAMES_PER_TX_MESSAGE = 32
SEED = 20261009


def c_can_id(msg) -> str:
    if msg.is_extended_frame:
        return f"(0x{msg.frame_id:08X}UL | CAN_ID_EXTENDED_FLAG)"
    return f"0x{msg.frame_id:03X}UL"


def c_bytes(data: bytes) -> str:
    padded = bytes(data) + bytes(8 - len(data))
    return "{ " + ", ".join(f"0x{b:02X}U" for b in padded) + " }"


def random_raw(sig, rng: random.Random) -> int:
    if sig.is_signed:
        return rng.randint(-(1 << (sig.length - 1)), (1 << (sig.length - 1)) - 1)
    return rng.randint(0, (1 << sig.length) - 1)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dbc", required=True, type=Path)
    ap.add_argument("--routes", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    routes = json.loads(args.routes.read_text(encoding="utf-8"))
    node = routes["node"]
    gw_targets = {r["tx"] for r in routes.get("pdu_gateway", [])}
    db = cantools.database.load_file(str(args.dbc))
    rng = random.Random(SEED)
    rx_lines, tx_lines = [], []

    for msg in sorted(db.messages, key=lambda m: m.frame_id):
        rx_signals = [s for s in msg.signals if node in s.receivers]
        if rx_signals:
            for _ in range(FRAMES_PER_RX_MESSAGE):
                data = bytes(rng.randrange(256) for _ in range(msg.length))
                decoded = msg.decode(data, decode_choices=False, scaling=False)
                for sig in rx_signals:
                    rx_lines.append(
                        f"    {{ {c_can_id(msg)}, {msg.length}U, {c_bytes(data)}, "
                        f"ComConf_ComSignal_{sig.name}, {int(decoded[sig.name])}LL }},")

    for msg in sorted(db.messages, key=lambda m: m.frame_id):
        # PDU gateway targets have no Com I-PDU, so only Com TX messages get vectors
        if msg.senders != [node] or msg.name in gw_targets:
            continue
        for _ in range(FRAMES_PER_TX_MESSAGE):
            raw = {s.name: random_raw(s, rng) for s in msg.signals}
            data = msg.encode(raw, scaling=False, strict=False)
            values = ", ".join(f"{{ ComConf_ComSignal_{n}, {v}LL }}" for n, v in raw.items())
            tx_lines.append(
                f"    {{ ComConf_ComIPdu_{msg.name}, {c_can_id(msg)}, {len(raw)}U, "
                f"{{ {values} }}, {msg.length}U, {c_bytes(data)} }},")

    out = [
        "/*",
        " * GENERATED FILE - DO NOT EDIT.",
        f" * Source: {args.dbc.as_posix()} (node {node}), oracle: cantools {cantools.__version__}",
        " * Generator: tools/gen_test_vectors.py",
        " */",
        "#ifndef TEST_VECTORS_H",
        "#define TEST_VECTORS_H",
        "",
        '#include "Com_Cfg.h"',
        '#include "Can_GeneralTypes.h"',
        "",
        "typedef struct { Can_IdType canId; uint8 dlc; uint8 data[8]; Com_SignalIdType signalId; long long expected; } RxVector;",
        "typedef struct { Com_SignalIdType signalId; long long raw; } TxSignalValue;",
        "typedef struct { PduIdType pduId; Can_IdType canId; uint8 numSignals; TxSignalValue signals[8]; uint8 dlc; uint8 expected[8]; } TxVector;",
        "",
        "static const RxVector rx_vectors[] =",
        "{",
        *rx_lines,
        "};",
        "",
    ]
    out += ["static const TxVector tx_vectors[] =", "{", *tx_lines, "};", "",
            "#endif /* TEST_VECTORS_H */", ""]
    args.out.write_text("\n".join(out), encoding="utf-8")
    print(f"gen_test_vectors: {len(rx_lines)} RX checks, {len(tx_lines)} TX frames -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
