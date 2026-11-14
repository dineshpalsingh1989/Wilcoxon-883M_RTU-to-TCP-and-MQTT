"""Ensure board folders are actual independent projects, not navigation placeholders."""
import configparser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = {
    '.': {'adafruit_feather_esp32s3': 'ESP32 Family/ESP32-S3',
          'cytron_maker_esp32': 'ESP32 Family/Cytron Maker ESP32',
          'cytron_iriv_io_controller': 'IRIV IOC'},
    'ILA210': {'esp32s3': 'ESP32 Family/ESP32-S3',
               'cytron_iriv_io_controller': 'IRIV IOC'},
}


class BoardLayoutTests(unittest.TestCase):
    def test_standalone_projects(self):
        for project, boards in PROJECTS.items():
            for name, folder in boards.items():
                board = ROOT / project / folder
                with self.subTest(board=str(board)):
                    cfg = configparser.ConfigParser(interpolation=None)
                    cfg.read(board / 'platformio.ini')
                    self.assertEqual(cfg['platformio']['default_envs'], name)
                    self.assertEqual(cfg['platformio']['extra_configs'], 'profile.ini')
                    self.assertNotIn('select_board.py', cfg['env']['extra_scripts'])
                    self.assertTrue((board / 'src/main.cpp').is_file())
                    self.assertFalse((board / 'src').is_symlink())
                    self.assertTrue((board / 'scripts/web_installer.py').is_file())
                    self.assertTrue((board / 'lib').is_dir())
                    if project == '.':
                        self.assertTrue((board / 'lib/Wilcoxon883MRTU/src/Wilcoxon883MRTU.cpp').is_file())
                        self.assertTrue((board / 'src/web/web_dashboard.cpp').is_file())
                    else:
                        self.assertTrue((board / 'src/ioc/pages.h').is_file())

    def test_root_builds_select_board_owned_sources(self):
        class Env(dict):
            def subst(self, text):
                return self[text[1:]]

            def Replace(self, **values):
                self.update(values)

            def Prepend(self, **values):
                self.update(values)

        for project, boards in PROJECTS.items():
            directory = (ROOT / project).resolve()
            script = directory / 'scripts/select_board.py'
            for name, folder in boards.items():
                env = Env(PROJECT_DIR=str(directory), PIOENV=name)
                exec(compile(script.read_text(), str(script), 'exec'),
                     {'Import': lambda _: None, 'env': env})
                self.assertEqual(Path(env['PROJECT_SRC_DIR']), directory / folder / 'src')
                self.assertIn(str(directory / folder / 'lib'), env['LIBSOURCE_DIRS'])


if __name__ == '__main__':
    unittest.main()
