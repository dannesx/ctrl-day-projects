import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('oled', ROOT / 'oled.py')
oled = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oled)

class ExportTests(unittest.TestCase):
    def test_seed_geometry_and_png_roundtrip(self):
        project = oled.validate(json.loads((ROOT / 'screens/jokenpo.json').read_text()))
        self.assertEqual(len(project['screens']), 17)
        for screen in project['screens']:
            pixels = screen['pixels']
            if screen['name'] in ('JO', 'KEN', 'PO!!'):
                self.assertFalse(any(pixels[:2048]))
            data = oled.png(pixels)
            self.assertEqual(struct.unpack('>II', data[16:24]), (128, 64))
            offset, compressed = 8, b''
            while offset < len(data):
                size = int.from_bytes(data[offset:offset+4], 'big')
                kind = data[offset+4:offset+8]
                payload = data[offset+8:offset+8+size]
                self.assertEqual(zlib.crc32(kind+payload), int.from_bytes(data[offset+8+size:offset+12+size], 'big'))
                if kind == b'IDAT':
                    compressed += payload
                offset += size+12
            raw = zlib.decompress(compressed)
            for y in range(64):
                self.assertEqual(raw[y*385], 0)
                for x in range(128):
                    start = y*385+1+x*3
                    expected = ((255,227,83) if y < 16 else (57,202,255)) if pixels[y*128+x] else (0,0,0)
                    self.assertEqual(tuple(raw[start:start+3]), expected)

    def test_browser_and_python_export_match(self):
        script = "const O=require('./core.js'),p=require('./screens/jokenpo.json');console.log(JSON.stringify(p.screens.map(s=>O.code(s))));"
        headers = json.loads(subprocess.check_output(['node','-e',script], cwd=ROOT))
        screens = json.loads((ROOT / 'screens/jokenpo.json').read_text())['screens']
        for s, js in zip(screens, headers):
            py = oled.header(s['pixels'], s['id'])
            self.assertEqual(re.findall(r'0x[0-9A-F]{2}', js), re.findall(r'0x[0-9A-F]{2}', py))
            self.assertEqual(re.findall(r'static void (\w+)', js), re.findall(r'static void (\w+)', py))

if __name__ == '__main__':
    unittest.main()
