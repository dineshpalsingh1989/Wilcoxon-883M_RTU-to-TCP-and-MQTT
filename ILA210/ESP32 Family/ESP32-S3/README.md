# Wilcoxon ILA210 / ESP32-S3 DevKitC-1-N8

This folder holds the board-specific PlatformIO configuration. Shared firmware stays in the sensor project's `src/` folder. The project-root `platformio.ini` loads this profile automatically. Open the sensor project root in PlatformIO, rather than this profile folder.

From the Wilcoxon workspace root:

```powershell
pio run -d ILA210 -e esp32s3
```

For an authorized hardware upload, use the same command with `-t upload`. Check the sensor project's wiring guide first.
