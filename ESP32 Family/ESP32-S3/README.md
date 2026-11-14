# Wilcoxon 883M / ESP32-S3

This is a complete, independent PlatformIO project. Edit this board's files here; changes do not automatically modify other boards.

- [src/main.cpp](src/main.cpp): firmware entry point
- [src/](src/): HTML/web pages, sensor communication and hardware code
- [lib/](lib/): local libraries; downloaded dependencies are declared in `platformio.ini`
- [scripts/](scripts/): build, version and installer helpers
- `platformio.ini`: standalone project configuration
- `profile.ini`: board settings also used by the sensor-root build selector

From the Wilcoxon workspace root:

```powershell
pio run -d "ESP32 Family/ESP32-S3"
```

Open this folder directly in PlatformIO to build it independently. Review the sensor wiring guide before any hardware upload.

HTML and HTTP handlers are in [src/web/](src/web/). Sensor communication is in [src/modbus/](src/modbus/) and [lib/Wilcoxon883MRTU/](lib/Wilcoxon883MRTU/).
