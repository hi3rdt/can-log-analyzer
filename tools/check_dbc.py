#!/usr/bin/env python3
"""Check a hand-written DBC against a CAN log (DBC track, exercise D1).

Reads a SavvyCAN GVRET CSV or a candump -l log, decodes every frame whose ID
is in the DBC with cantools, and prints one block per message:

  - frame count, DLC mismatches and the cycle time seen in the log
    (median gap) next to GenMsgCycleTime from the DBC, if set
  - per signal: min .. max of the decoded physical value, and how many
    frames fall outside the [min|max] range declared in the DBC
  - for signals whose name contains "Counter": how many consecutive frames
    step by exactly +1 (modulo 2^length), repeat, or jump

The DBC itself is loaded in strict mode, so overlapping signals or signals
outside the DLC are reported as errors. Checksum and CRC algorithms are not
guessed here: verify them as in docs/can-frame-basics.vi.md.

Usage:
  python tools/check_dbc.py --dbc dbc/gvret_log.dbc --log GVRET_Log.csv
"""
import argparse
import re
import statistics
import sys

import cantools

CANDUMP_RE = re.compile(r"^\((\d+\.\d+)\)\s+\S+\s+([0-9A-Fa-f]+)#([0-9A-Fa-f]*)")


def read_log(path):
    """Return a list of (timestamp_us, can_id, data) from GVRET CSV or candump."""
    frames = []
    with open(path, encoding="utf-8") as f:
        first = f.readline()
        if first.startswith("Time Stamp"):
            for line in f:
                cols = line.strip().split(",")
                if len(cols) < 5:
                    continue
                length = int(cols[4])
                data = bytes(int(b, 16) for b in cols[5:5 + length])
                frames.append((int(cols[0]), int(cols[1], 16), data))
        else:
            for line in [first] + f.readlines():
                m = CANDUMP_RE.match(line.strip())
                if m:
                    frames.append((round(float(m.group(1)) * 1e6), int(m.group(2), 16),
                                   bytes.fromhex(m.group(3))))
    return frames


def counter_steps(values, bits):
    """Count +1 steps, repeats and jumps between consecutive counter values."""
    mod = 1 << bits
    step = sum(1 for a, b in zip(values, values[1:]) if (a + 1) % mod == b)
    same = sum(1 for a, b in zip(values, values[1:]) if a == b)
    return step, same, max(len(values) - 1, 0) - step - same


def check(db, frames, out):
    """Write the report to out; return the number of messages that had problems."""
    problems = 0
    for msg in sorted(db.messages, key=lambda m: m.frame_id):
        own = [f for f in frames if f[1] == msg.frame_id]
        out.write(f"\n{msg.name} (0x{msg.frame_id:X}, DLC {msg.length}): {len(own)} frames\n")
        if not own:
            out.write("  not in log\n")
            problems += 1
            continue
        bad_dlc = [f for f in own if len(f[2]) != msg.length]
        good = [f for f in own if len(f[2]) == msg.length]
        if bad_dlc:
            out.write(f"  DLC mismatch in {len(bad_dlc)} frames\n")
            problems += 1
        gaps = [b[0] - a[0] for a, b in zip(own, own[1:]) if b[0] > a[0]]
        dbc_cycle = msg.cycle_time if msg.cycle_time else "-"
        log_cycle = f"{statistics.median(gaps) / 1000:.1f} ms" if gaps else "-"
        out.write(f"  cycle: log {log_cycle}, DBC {dbc_cycle}\n")
        decoded = [msg.decode(f[2], decode_choices=False) for f in good]
        for sig in msg.signals:
            vals = [d[sig.name] for d in decoded]
            if not vals:
                continue
            line = f"  {sig.name:<24} {min(vals):>12g} .. {max(vals):<12g} {sig.unit or ''}"
            if sig.minimum is not None and sig.maximum is not None and sig.maximum > sig.minimum:
                outside = sum(1 for v in vals if v < sig.minimum or v > sig.maximum)
                if outside:
                    line += f"  {outside} outside [{sig.minimum:g}|{sig.maximum:g}]"
                    problems += 1
            out.write(line + "\n")
            if "counter" in sig.name.lower():
                raw = [int(round((v - sig.offset) / sig.scale)) for v in vals]
                step, same, jump = counter_steps(raw, sig.length)
                out.write(f"  {'':<24} counter: {step} x +1, {same} repeats, {jump} jumps\n")
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dbc", required=True)
    ap.add_argument("--log", required=True, help="GVRET CSV or candump -l log")
    args = ap.parse_args(argv)
    try:
        db = cantools.database.load_file(args.dbc, strict=True)
    except cantools.database.UnsupportedDatabaseFormatError as exc:
        print(f"{args.dbc}: {exc}")
        return 2
    frames = read_log(args.log)
    print(f"{args.log}: {len(frames)} frames, {len({f[1] for f in frames})} IDs; "
          f"{args.dbc}: {len(db.messages)} messages")
    return 1 if check(db, frames, sys.stdout) else 0


if __name__ == "__main__":
    sys.exit(main())
