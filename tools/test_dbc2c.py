"""Unit tests for dbc2c.py (python3 -m unittest discover -s tools)."""
import unittest
from pathlib import Path

import dbc2c

ROOT = Path(__file__).resolve().parent.parent
GATEWAY_DBC = ROOT / "config" / "gateway.dbc"


def dbc(body: str) -> str:
    return 'VERSION ""\n\nBU_: A GW\n\n' + body


class ParseTests(unittest.TestCase):
    def test_gateway_dbc_parses(self):
        msgs = dbc2c.parse_dbc(GATEWAY_DBC.read_text())
        dbc2c.validate(msgs)
        names = [m.name for m in msgs]
        self.assertIn("EngineData", names)
        ext = next(m for m in msgs if m.name == "DiagExtended")
        self.assertTrue(ext.is_extended)

    def test_motorola_bit_walk(self):
        sig = dbc2c.parse_dbc(dbc(
            'BO_ 1 M: 8 A\n SG_ S : 37|11@0- (1,0) [0|0] "" GW\n'))[0].signals[0]
        self.assertEqual(sig.bit_positions(),
                         [37, 36, 35, 34, 33, 32, 47, 46, 45, 44, 43])
        self.assertEqual(sig.c_type, "COM_SINT16")

    def test_overlap_rejected(self):
        msgs = dbc2c.parse_dbc(dbc(
            'BO_ 1 M: 2 A\n SG_ X : 0|8@1+ (1,0) [0|0] "" GW\n'
            ' SG_ Y : 4|8@1+ (1,0) [0|0] "" GW\n'))
        with self.assertRaisesRegex(dbc2c.DbcError, "overlaps"):
            dbc2c.validate(msgs)

    def test_outside_dlc_rejected(self):
        msgs = dbc2c.parse_dbc(dbc('BO_ 1 M: 1 A\n SG_ X : 4|8@1+ (1,0) [0|0] "" GW\n'))
        with self.assertRaisesRegex(dbc2c.DbcError, "outside DLC"):
            dbc2c.validate(msgs)

    def test_multiplexed_rejected(self):
        with self.assertRaisesRegex(dbc2c.DbcError, "multiplexed"):
            dbc2c.parse_dbc(dbc('BO_ 1 M: 8 A\n SG_ X M : 0|8@1+ (1,0) [0|0] "" GW\n'))

    def test_can_fd_rejected(self):
        with self.assertRaisesRegex(dbc2c.DbcError, "CAN FD"):
            dbc2c.parse_dbc(dbc("BO_ 1 M: 64 A\n"))

    def test_init_value_physical_zero(self):
        sig = dbc2c.parse_dbc(dbc(
            'BO_ 1 M: 8 A\n SG_ T : 0|8@1+ (1,-40) [-40|215] "" GW\n'))[0].signals[0]
        self.assertEqual(sig.raw_init_value(), 40)


class RoutingTests(unittest.TestCase):
    def setUp(self):
        self.msgs = dbc2c.parse_dbc(GATEWAY_DBC.read_text())

    def test_gateway_target_is_not_a_com_pdu(self):
        cfg = dbc2c.build_config(self.msgs, "GW",
                                 [{"rx": "BodyStatus", "tx": "GW_BodyStatusFwd"}])
        com_names = [m.name for m, _ in cfg.com_ipdus]
        self.assertNotIn("GW_BodyStatusFwd", com_names)
        body = [m.name for m in cfg.canif_rx].index("BodyStatus")
        self.assertEqual(cfg.pdur_rx_dests[body],
                         [("PDUR_DEST_COM", com_names.index("BodyStatus")),
                          ("PDUR_DEST_CANIF", [m.name for m in cfg.canif_tx].index("GW_BodyStatusFwd"))])

    def test_canif_rx_sorted_by_id(self):
        cfg = dbc2c.build_config(self.msgs, "GW", [])
        ids = [m.can_id for m in cfg.canif_rx]
        self.assertEqual(ids, sorted(ids))

    def test_unknown_gateway_message(self):
        with self.assertRaisesRegex(dbc2c.DbcError, "unknown message"):
            dbc2c.build_config(self.msgs, "GW", [{"rx": "Nope", "tx": "GW_BodyStatusFwd"}])


class CantoolsCrossCheck(unittest.TestCase):
    def test_signal_layout_matches_cantools(self):
        try:
            import cantools
        except ImportError:
            self.skipTest("cantools not installed")
        ref = cantools.database.load_file(str(GATEWAY_DBC))
        for msg in dbc2c.parse_dbc(GATEWAY_DBC.read_text()):
            ref_msg = ref.get_message_by_name(msg.name)
            self.assertEqual(msg.can_id & 0x1FFFFFFF, ref_msg.frame_id)
            self.assertEqual(msg.is_extended, ref_msg.is_extended_frame)
            for sig in msg.signals:
                r = ref_msg.get_signal_by_name(sig.name)
                self.assertEqual((sig.start_bit, sig.length, sig.is_signed, sig.little_endian),
                                 (r.start, r.length, r.is_signed, r.byte_order == "little_endian"))


if __name__ == "__main__":
    unittest.main()
