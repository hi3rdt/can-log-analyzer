# ESP32 rest-bus simulator (Project 1)

Replays `samples/demo_drive.csv` (30 s drive, generated from `config/gateway.dbc`) on CAN at
500 kbit/s with the log's original timing, standing in for the ABS, ECM and BCM of a car so the
STM32 instrument cluster can be tested on a desk. Faults are injected per CAN ID from the serial
console. Design: [docs/projects/p1-instrument-cluster.vi.md](../../docs/projects/p1-instrument-cluster.vi.md).

| ID | Message | Period | Cluster uses |
|---|---|---|---|
| `0x100` | VehicleSpeed | 10 ms | speed, `SpeedCounter` (alive counter), `SpeedChecksum` (nibble XOR) |
| `0x200` | EngineData | 20 ms | RPM, coolant temperature |
| `0x300` | BodyStatus | 100 ms | door open lamp |
| `0x2F0` | (undocumented, see SavvyCAN exercise) | 100 ms | turn signal, headlamp, battery voltage |
| `0x18FEEF00` | DiagExtended (29-bit) | 1000 ms | odometer, ambient temperature |

## Build and flash (ESP-IDF v5.x)

```bash
cd esp32/restbus_sim
idf.py set-target esp32
idf.py -p COM5 flash monitor        # Linux: -p /dev/ttyUSB0
```

Pins and queue sizes: `main/app_config.h` (TX = GPIO5, RX = GPIO4).

## Console commands

| Command | Effect | What the cluster should show |
|---|---|---|
| `status` | replay + TWAI state, TEC/REC, counters, active faults | |
| `play` / `pause` | start / stop the replay | pause > timeout: every gauge "---" |
| `speed <25..1000>` | replay speed in % of real time | |
| `fault 100 drop` | stop sending 0x100 | speed timeout -> "---" + warning lamp |
| `fault 100 corrupt` | flip bit 7 of the last byte (checksum nibble) | speed INVALID after N bad frames |
| `fault 100 freeze` | repeat the first payload forever | alive counter stuck -> INVALID |
| `fault 100 repeat` | send every frame twice | counter repeated (step 0) detected |
| `fault 100 none` / `clear` | remove one / all faults | recovers after K good frames |
| `silence on\|off` | send nothing at all | all ECUs dead |
| `rxlog on\|off` | print frames received from the bus | |
| `mode noack` / `mode normal` | reboot into self-test (no ACK needed) / normal mode | |

## Self-test without the STM32

With only one node on the bus nobody acknowledges the frames: the ESP32 goes error passive and keeps
retransmitting. To check the ESP32 side alone, run `mode noack` then `rxlog on`: frames are sent
without ACK and received back (self-reception). Without a transceiver, a wire from GPIO5 to GPIO4
also works for this test. `mode normal` (or a power cycle) returns to normal mode.

## Tests

The log parser and fault injection are plain C and are unit-tested on the host from the repository
root (`test/host/test_restbus.c`, run by `ctest --preset host` and in CI).
