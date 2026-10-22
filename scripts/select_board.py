"""Select physical board-owned sources while preserving root CI commands/artifact paths."""
from pathlib import Path
Import("env")
BOARDS = {'adafruit_feather_esp32s3': 'ESP32 Family/ESP32-S3', 'cytron_maker_esp32': 'ESP32 Family/Cytron Maker ESP32', 'cytron_iriv_io_controller': 'IRIV IOC'}
root = Path(env.subst("$PROJECT_DIR"))
board = root / BOARDS[env["PIOENV"]]
assert (board / "src/main.cpp").is_file(), "Missing board firmware"
env.Replace(PROJECT_SRC_DIR=str(board / "src"), PROJECT_INCLUDE_DIR=str(board / "include"))
env.Prepend(CPPPATH=[str(board / "src"), str(board / "include")], LIBSOURCE_DIRS=[str(board / "lib")])
