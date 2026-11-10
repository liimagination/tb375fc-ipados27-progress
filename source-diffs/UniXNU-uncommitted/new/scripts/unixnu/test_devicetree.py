"""Regression coverage for XNU interrupt-controller handle publication."""
import struct
import unittest

from .devicetree import FdtNode, hardware_properties, node_phandle
from .util import Error


class PhandleTests(unittest.TestCase):
    def controller(self, **properties):
        return FdtNode('intc', properties={
            '#interrupt-cells': struct.pack('>I', 3),
            'interrupt-controller': b'', **properties})

    def test_fdt_handle_is_published_in_xnu_namespace_little_endian(self):
        controller = self.controller(phandle=struct.pack('>I', 0x01020304))
        result = dict(hardware_properties(controller, {}))
        self.assertEqual(result['AAPL,phandle'], b'\x04\x03\x02\x01')
        self.assertEqual(result['phandle'], result['AAPL,phandle'])

    def test_linux_only_handle_is_supported(self):
        controller = self.controller(**{'linux,phandle': struct.pack('>I', 7)})
        self.assertEqual(node_phandle(controller), 7)
        self.assertEqual(dict(hardware_properties(controller, {}))['AAPL,phandle'], struct.pack('<I', 7))

    def test_matching_aliases_emit_one_apple_handle(self):
        controller = self.controller(phandle=struct.pack('>I', 7),
                                     **{'linux,phandle': struct.pack('>I', 7)})
        result = hardware_properties(controller, {})
        self.assertEqual([value for key, value in result if key == 'AAPL,phandle'], [struct.pack('<I', 7)])

    def test_invalid_handles_are_rejected(self):
        for props in ({'phandle': struct.pack('>I', 0)},
                      {'phandle': struct.pack('>I', 7), 'linux,phandle': struct.pack('>I', 8)}):
            with self.subTest(props=props), self.assertRaises(Error):
                hardware_properties(self.controller(**props), {})

    def test_interrupt_parent_matches_xnu_registered_controller(self):
        controller = self.controller(phandle=struct.pack('>I', 7))
        client = FdtNode('timer', properties={
            'interrupt-parent': struct.pack('>I', 7),
            'interrupts': struct.pack('>III', 1, 14, 4)})
        provider = dict(hardware_properties(controller, {7: controller}))
        consumer = dict(hardware_properties(client, {7: controller}))
        self.assertEqual(consumer['interrupt-parent'], provider['AAPL,phandle'])
        self.assertEqual(len(consumer['interrupts']), 3 * 4)


if __name__ == '__main__':
    unittest.main()
