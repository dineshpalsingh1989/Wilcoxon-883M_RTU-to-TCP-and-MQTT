# Wilcoxon883MRTU 0.1.0-beta.1

**Beta / experimental library for ESP32 + Wilcoxon 883M over RS485 Modbus RTU.**

Download `Wilcoxon883MRTU-0.1.0-beta.1.zip` and install it using Arduino IDE's **Sketch > Include Library > Add .ZIP Library**, then open **File > Examples > Wilcoxon883MRTU > BasicRead**. PlatformIO users can extract the library into a separate project's `lib/Wilcoxon883MRTU` directory.

This download contains **library source and an example**, not a flashable gateway `.bin`. Installing or downloading it does not update an ESP32. It is compiled into a sketch only when that sketch includes and uses `Wilcoxon883MRTU.h`. Uploading BasicRead replaces the application on the selected board; use a bench board or keep the gateway firmware available to restore.

The main gateway firmware still uses its original Modbus driver. Its normal PlatformIO upload and GitHub firmware updater do not install this library beta.

Included: function 03/04 reads (up to 125 registers), function 06 writes, CRC and response validation, explicit error results, and basic acceleration/velocity/temperature helpers. No TCP, Wi-Fi, MQTT or dashboard dependency.

Checks: native C++ tests with simulated UART transport and an ESP32-S3 compilation of BasicRead. Real 883M communication, electrical timing, fault recovery and long-duration operation remain unvalidated. This is not production-ready firmware or a complete waveform/FFT capture library.

Manual defaults are 9600 baud, 8N1, slave 1. Match your actual sensor settings and board wiring. Intended model: **883M**, not an unverified 833M variant.

Read the [library guide](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/blob/main/experiments/Wilcoxon883MRTU-beta/README.md) before connecting hardware. `SHA256SUMS.txt` provides the ZIP checksum.
