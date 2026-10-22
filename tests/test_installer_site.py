import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('installer', ROOT / 'scripts/build_installer_site.py')
site = importlib.util.module_from_spec(spec)
spec.loader.exec_module(site)


class InstallerTests(unittest.TestCase):
    def assemble(self, ila):
        def gh(*args):
            if args[:2] == ('repo', 'view'):
                return 'https://github.com/example/gateway'
            return json.dumps({'assets': [{'name': 'firmware-ila210-cytron_iriv_io_controller.uf2'}] if ila else []})

        def unpack(tag, board, target, prefix=''):
            if prefix and not ila:
                return None
            target.mkdir(parents=True)
            (target / 'firmware.bin').write_bytes(b'ILA' if prefix else b'883M')
            return {'name': 'gateway', 'builds': [{'chipFamily': 'ESP32' if board == 'cytron_maker_esp32' else 'ESP32-S3',
                    'parts': [{'path': 'firmware.bin', 'offset': 65536}]}]}

        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / 'site'
            with patch.object(site, 'gh', gh), patch.object(site, 'pick_channels', return_value=[('stable', 'v5.0.0')]), patch.object(site, 'unpack_board', unpack):
                site.main(out)
            channels = json.loads((out / 'channels.json').read_text())['channels']
            self.assertEqual([c['sensor'] for c in channels], ['883m', 'ila210'] if ila else ['883m'])
            for c in channels:
                manifest = json.loads((out / c['manifest']).read_text())
                for build in manifest['builds']:
                    for part in build['parts']:
                        payload = (out / c['manifest']).parent / part['path']
                        self.assertEqual(payload.read_bytes(), b'ILA' if c['sensor'] == 'ila210' else b'883M')
                if c['sensor'] == 'ila210':
                    self.assertEqual([b['chipFamily'] for b in manifest['builds']], ['ESP32-S3'])
                    self.assertIn('firmware-ila210-', c['uf2']['cytron_iriv_io_controller'])

    def test_separate_images(self):
        self.assemble(True)

    def test_missing_ila_never_uses_883m(self):
        self.assemble(False)


if __name__ == '__main__':
    unittest.main()
