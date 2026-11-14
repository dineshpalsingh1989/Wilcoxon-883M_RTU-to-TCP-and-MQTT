# Board-owned firmware projects

Each board is a complete independent PlatformIO project. Open the board folder directly to trace and edit its implementation.

| Sensor | Board project |
| --- | --- |
| 883M | `ESP32 Family/ESP32-S3/` |
| 883M | `ESP32 Family/Cytron Maker ESP32/` |
| 883M | `IRIV IOC/` |
| ILA210 | `ILA210/ESP32 Family/ESP32-S3/` |
| ILA210 | `ILA210/IRIV IOC/` |

Inside each board project:

```text
src/main.cpp        Firmware startup and application
src/                Web/HTML, protocol, hardware and other firmware modules
lib/                Board-local libraries
include/            Project headers; generated trust headers stay untracked
scripts/            Board-local build/version/installer helpers
platformio.ini      Standalone project entry point and dependency declarations
profile.ini         MCU, board flags and board-specific options
README.md           Navigation and build command
```

883M HTML lives in `src/web/`. ILA210 sensor HTML is embedded in `src/main.cpp`, IOC HTML in `src/ioc/pages.h`, and sensor communication settings in `src/web/sensor_health_page.h`.

The 883M Modbus library source is copied into each 883M project's `lib/Wilcoxon883MRTU/`, including its license. Third-party libraries are declared in each project's `platformio.ini`; PlatformIO downloads them into that project's `.pio/libdeps/`. Framework/toolchain packages are managed by PlatformIO. Downloaded dependencies, build output and generated certificates are excluded from Git.

## Build independently

From the workspace root:

```powershell
pio run -d "ESP32 Family/ESP32-S3"
pio run -d "ESP32 Family/Cytron Maker ESP32"
pio run -d "IRIV IOC"
pio run -d "ILA210/ESP32 Family/ESP32-S3"
pio run -d "ILA210/IRIV IOC"
```

Each standalone project has exactly one default environment. No upload or physical output action is implied by a build.

## Root compatibility commands

Existing sensor-root environment names remain available, so CI and release asset locations remain compatible. `scripts/select_board.py` selects the correct physical `src/`, `include/` and `lib/`; `scripts/board_helpers.py` runs that board's pre-build helper. The root installer script collects the selected board's built image.

```powershell
pio run -e adafruit_feather_esp32s3
pio run -e cytron_maker_esp32
pio run -e cytron_iriv_io_controller
pio run -d ILA210 -e esp32s3
pio run -d ILA210 -e cytron_iriv_io_controller
```

Board projects are independent copies, not symlinks or wrappers around shared source. Apply a common fix to every affected board and test those targets. A board-specific edit stays with that board. The library under `experiments/Wilcoxon883MRTU-beta/` remains a development project; firmware uses the copy inside the selected board.

The root `src/README.md` files are navigation notes only. Demos read an explicit board's actual HTML; they remain simulations and do not validate physical I/O.

## Verification of this reorganization

On 2026-10-22, all five standalone board projects compiled successfully. The root-compatible 883M ESP32-S3 build also passed using the selected board's physical source and library folders. Four Python layout/installer tests, root UI smoke checks, the ILA dashboard/IOC/sensor-health/demo browser suites, and native IOC logic/sensor-policy tests passed after their source paths were updated. No hardware was flashed or driven.

Local source copies were compared byte-for-byte with the working source snapshots. Existing local feature edits were retained locally. The GitHub reorganization relocates the previously published `main` source into each board, preserving its firmware contents rather than introducing unrelated local edits. Source history remains available in Git; local pre-move snapshots are kept outside tracked files.
