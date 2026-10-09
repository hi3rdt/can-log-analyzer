#!/usr/bin/env python3
"""Generate a deterministic 30 s "demo drive" CAN log for SavvyCAN practice.

The log contains the input PDUs of config/gateway.dbc (ABS, ECM, BCM) with a
believable driving profile, plus one frame (0x2F0) that is deliberately NOT in
the DBC so it can be reverse-engineered as an exercise. Gateway output PDUs
(GW_*) are not generated: the STM32 produces those later in the project.

Outputs:
  <out>.csv  SavvyCAN native (GVRET v2) CSV, timestamps in microseconds
  <out>.log  candump -l format, readable by SavvyCAN, cantools and can-utils

Usage:
  python tools/gen_demo_log.py --dbc config/gateway.dbc --out samples/demo_drive
"""
import argparse
import math
import random

import cantools

DURATION_S = 30.0
SEED = 2026                    # fixed seed: the committed logs are reproducible
JITTER_US = 200                # +/- send jitter, like a real ECU task scheduler

# Cycle times in ms (typical values for these message types)
CYCLE_MS = {
    "VehicleSpeed": 10,
    "EngineData": 20,
    "BodyStatus": 100,
    "DiagExtended": 1000,
}
MYSTERY_ID = 0x2F0
MYSTERY_CYCLE_MS = 100

GEAR_RPM_PER_KMH = [110.0, 60.0, 40.0, 30.0, 25.0]
GEAR_UPPER_KMH = [15.0, 30.0, 50.0, 70.0, 1000.0]


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def vehicle_state(t, rng):
    """Physical vehicle state at time t (seconds)."""
    engine_on = t >= 1.0

    # Speed profile: idle, accelerate, cruise, brake, stop
    if t < 4.0:
        speed = 0.0
    elif t < 14.0:
        speed = 80.0 * (1.0 - math.cos(math.pi * (t - 4.0) / 10.0)) / 2.0
    elif t < 20.0:
        speed = 80.0
    elif t < 27.0:
        speed = 80.0 * (1.0 + math.cos(math.pi * (t - 20.0) / 7.0)) / 2.0
    else:
        speed = 0.0

    # Left curve between 16 s and 19 s (positive angle = left)
    steering = 45.0 * math.sin(math.pi * (t - 16.0) / 3.0) if 16.0 <= t < 19.0 else 0.0
    steering += rng.uniform(-0.3, 0.3)

    if not engine_on:
        rpm = 0.0
    elif t < 2.0:
        rpm = 250.0 + 550.0 * (t - 1.0)          # crank and run-up to idle
    else:
        gear = next(i for i, upper in enumerate(GEAR_UPPER_KMH) if speed < upper)
        rpm = max(800.0, speed * GEAR_RPM_PER_KMH[gear])
    rpm += rng.uniform(-8.0, 8.0) if rpm > 0.0 else 0.0

    if 4.0 <= t < 14.0:
        throttle, torque = 35.0, 120.0
    elif 14.0 <= t < 20.0:
        throttle, torque = 18.0, 40.0
    elif 20.0 <= t < 27.0:
        throttle, torque = 0.0, -30.0            # engine braking: signed torque
    else:
        throttle, torque = (3.0, 10.0) if engine_on else (0.0, 0.0)

    coolant = 20.0 + (min(20.0, (t - 1.0) / 1.5) if engine_on else 0.0)

    # Outer (right) wheel turns faster in a left curve
    curve_delta = 0.04 * speed * max(0.0, steering) / 45.0
    wheel_fl = max(0.0, speed - curve_delta + rng.uniform(-0.3, 0.3))
    wheel_fr = max(0.0, speed + curve_delta + rng.uniform(-0.3, 0.3))

    return {
        "engine_on": engine_on,
        "speed": speed,
        "wheel_fl": wheel_fl,
        "wheel_fr": wheel_fr,
        "abs_active": 1 if 23.5 <= t < 25.0 else 0,
        "rpm": rpm,
        "coolant": coolant,
        "throttle": throttle,
        "torque": torque,
        "steering": steering,
        "door_fl": 1 if t >= 28.5 else 0,
        "turn_left": 15.5 <= t < 19.0,
        "headlamp": t >= 15.0,
    }


def nibble_xor_checksum(payload):
    """XOR of the 14 nibbles in bytes 0..6 and the counter nibble of byte 7."""
    acc = 0
    for b in payload[:7]:
        acc ^= (b >> 4) ^ (b & 0x0F)
    return acc ^ (payload[7] & 0x0F)


def build_frames(db):
    rng = random.Random(SEED)
    events = []                                  # (nominal_us, name or id)
    for name, cycle in CYCLE_MS.items():
        events += [(t, name) for t in range(0, int(DURATION_S * 1e6), cycle * 1000)]
    events += [(t, MYSTERY_ID)
               for t in range(3000, int(DURATION_S * 1e6), MYSTERY_CYCLE_MS * 1000)]
    events.sort(key=lambda e: e[0])

    frames = []                                  # (ts_us, can_id, is_ext, payload)
    counters = {"VehicleSpeed": 0, "Mystery": 0}

    for nominal_us, what in events:
        ts = max(0, nominal_us + rng.randint(-JITTER_US, JITTER_US))
        s = vehicle_state(nominal_us / 1e6, rng)

        if what == MYSTERY_ID:
            blink = s["turn_left"] and (int(nominal_us / 333000) % 2 == 0)  # 1.5 Hz
            vbat_cv = int(round((14.20 if s["engine_on"] else 12.40) * 100.0
                                + rng.uniform(-3.0, 3.0)))
            payload = bytearray(8)
            payload[0] = (0x01 if blink else 0) | (0x04 if s["headlamp"] else 0)
            payload[1] = 0x7C                    # constant: fuel level 62.0 %
            payload[2] = (vbat_cv >> 8) & 0xFF   # battery voltage, big endian, 0.01 V
            payload[3] = vbat_cv & 0xFF
            payload[4] = 0x5A
            payload[7] = counters["Mystery"] & 0x0F
            counters["Mystery"] += 1
            frames.append((ts, MYSTERY_ID, False, bytes(payload)))
            continue

        msg = db.get_message_by_name(what)
        if what == "VehicleSpeed":
            values = {
                "VehicleSpeed": s["speed"],
                "WheelSpeedFL": s["wheel_fl"],
                "WheelSpeedFR": s["wheel_fr"],
                "AbsActive": s["abs_active"],
                "SpeedCounter": counters["VehicleSpeed"] & 0x0F,
                "SpeedChecksum": 0,
            }
            counters["VehicleSpeed"] += 1
        elif what == "EngineData":
            values = {
                "EngineRpm": clamp(s["rpm"], 0.0, 16383.75),
                "CoolantTemp": round(s["coolant"]),
                "ThrottlePos": s["throttle"],
                "EngineTorque": s["torque"],
            }
        elif what == "BodyStatus":
            values = {
                "DoorFL": s["door_fl"], "DoorFR": 0, "DoorRL": 0, "DoorRR": 0,
                "SteeringAngle": s["steering"],
            }
        else:  # DiagExtended
            values = {"OdometerKm": 12345, "AmbientTemp": 27.5}

        payload = bytearray(msg.encode(values, strict=True))
        if what == "VehicleSpeed":
            payload[7] = (payload[7] & 0x0F) | (nibble_xor_checksum(payload) << 4)
        frames.append((ts, msg.frame_id, msg.is_extended_frame, bytes(payload)))

    frames.sort(key=lambda f: f[0])
    return frames


def write_gvret_csv(path, frames):
    with open(path, "w", newline="\n") as f:
        f.write("Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n")
        for ts, can_id, ext, data in frames:
            cells = ",".join(f"{b:02X}" for b in data)
            f.write(f"{ts},{can_id:08X},{'true' if ext else 'false'},Rx,0,{len(data)},{cells},\n")


def write_candump(path, frames):
    with open(path, "w", newline="\n") as f:
        for ts, can_id, ext, data in frames:
            id_txt = f"{can_id:08X}" if ext else f"{can_id:03X}"
            f.write(f"({ts // 1000000}.{ts % 1000000:06d}) can0 {id_txt}#{data.hex().upper()}\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dbc", required=True)
    ap.add_argument("--out", required=True, help="output path without extension")
    args = ap.parse_args()

    db = cantools.database.load_file(args.dbc)
    frames = build_frames(db)
    write_gvret_csv(args.out + ".csv", frames)
    write_candump(args.out + ".log", frames)
    print(f"{len(frames)} frames written to {args.out}.csv / .log")


if __name__ == "__main__":
    main()
