# Wilcoxon 883M / Adafruit Feather ESP32-S3

This folder holds the board-specific PlatformIO configuration. Shared firmware stays in the sensor project's `src/` folder. The project-root `platformio.ini` loads this profile automatically. Open the sensor project root in PlatformIO, rather than this profile folder.

From the Wilcoxon workspace root:

```powershell
pio run -d . -e adafruit_feather_esp32s3
```

For an authorized hardware upload, use the same command with `-t upload`. Check the sensor project's wiring guide first.
