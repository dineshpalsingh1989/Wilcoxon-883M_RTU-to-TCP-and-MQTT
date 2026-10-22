# Wilcoxon ILA210 / IRIV IOC

This is a complete, independent PlatformIO project. Edit this board's files here; changes do not automatically modify other boards.

- [src/main.cpp](src/main.cpp): firmware entry point
- [src/](src/): HTML/web pages, sensor communication and hardware code
- [lib/](lib/): local libraries; downloaded dependencies are declared in `platformio.ini`
- [scripts/](scripts/): build, version and installer helpers
- `platformio.ini`: standalone project configuration
- `profile.ini`: board settings also used by the sensor-root build selector

From the Wilcoxon workspace root:

```powershell
pio run -d "ILA210/IRIV IOC"
```

Open this folder directly in PlatformIO to build it independently. Review the sensor wiring guide before any hardware upload.

HTML locations: the sensor dashboard is embedded in [src/main.cpp](src/main.cpp); IOC pages are in [src/ioc/pages.h](src/ioc/pages.h), and communication settings in [src/web/sensor_health_page.h](src/web/sensor_health_page.h).
