"""Unit tests for check_dbc.py (python3 -m unittest discover -s tools)."""
import io
import tempfile
import unittest
from pathlib import Path

import cantools

import check_dbc

ROOT = Path(__file__).resolve().parent.parent

DBC = """VERSION ""

BU_: A

BO_ 256 Speed: 2 A
 SG_ Value : 0|8@1+ (0.5,0) [0|100] "km/h" Vector__XXX
 SG_ Counter : 8|4@1+ (1,0) [0|15] "" Vector__XXX
"""


class CheckDbcTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        (self.dir / "t.dbc").write_text(DBC)
        self.db = cantools.database.load_file(str(self.dir / "t.dbc"), strict=True)

    def tearDown(self):
        self.tmp.cleanup()

    def test_reads_gvret_csv(self):
        csv = self.dir / "t.csv"
        csv.write_text("Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n"
                       "1000,00000100,false,0,2,14,0E,\n")
        self.assertEqual(check_dbc.read_log(str(csv)), [(1000, 0x100, b"\x14\x0e")])

    def test_reads_candump(self):
        log = self.dir / "t.log"
        log.write_text("(0.010000) can0 100#140E\n")
        self.assertEqual(check_dbc.read_log(str(log)), [(10000, 0x100, b"\x14\x0e")])

    def test_counter_steps_wraps(self):
        self.assertEqual(check_dbc.counter_steps([14, 15, 0, 0, 3], 4), (2, 1, 1))

    def test_report_flags_range_and_counter(self):
        frames = [(i * 10000, 0x100, bytes([20, i % 16])) for i in range(20)]
        frames.append((200000, 0x100, bytes([250, 4])))      # 125 km/h > 100
        out = io.StringIO()
        self.assertEqual(check_dbc.check(self.db, frames, out), 1)
        text = out.getvalue()
        self.assertIn("1 outside [0|100]", text)
        self.assertIn("counter: 20 x +1", text)
        self.assertIn("cycle: log 10.0 ms", text)

    def test_demo_log_matches_gateway_dbc(self):
        db = cantools.database.load_file(str(ROOT / "config" / "gateway.dbc"))
        frames = check_dbc.read_log(str(ROOT / "samples" / "demo_drive.log"))
        out = io.StringIO()
        check_dbc.check(db, frames, out)
        self.assertIn("VehicleSpeed (0x100, DLC 8)", out.getvalue())


if __name__ == "__main__":
    unittest.main()
