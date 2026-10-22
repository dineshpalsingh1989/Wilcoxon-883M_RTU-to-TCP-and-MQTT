# Wilcoxon883MRTU 0.1.0-beta.4

**Beta / experimental library for ESP32 + Wilcoxon 883M: Modbus RTU sensor client plus an optional Modbus TCP server for PLCs.**

Download `Wilcoxon883MRTU-0.1.0-beta.4.zip` and install it using Arduino IDE's **Sketch > Include Library > Add .ZIP Library**. Examples: **AutoDiscover** for unknown sensor settings, **BasicRead** when settings are known, and **RtuToTcp** to publish sensor readings to PLCs over Modbus TCP. PlatformIO users can extract the library into a separate project's `lib/Wilcoxon883MRTU` directory.

This download contains **library source and examples**, not a flashable gateway `.bin`. Installing or downloading it does not update an ESP32. Uploading an example replaces the application on the selected board; use a bench board or keep the gateway firmware available to restore.

Gateway firmware from v4.10.0-beta.1 onwards is built with this library. Installing this ZIP does not change a gateway; a gateway gets a new library version only through a gateway firmware update.

New in beta.4: `Wilcoxon883MTCP`, a Modbus TCP server (functions 03, 04, 06 and 16) with up to eight clients, non-blocking sends, pipelined requests and bounded buffers. `RegisterBlocks` serves existing arrays at chosen addresses without copying them, with optional locking for multi-task programs. RTU-only sketches do not link the TCP code.

Included from earlier betas: read-only serial discovery, function 03/04 reads (up to 125 registers), function 06 writes, CRC and response validation, explicit error results, and basic acceleration/velocity/temperature helpers. No MQTT or dashboard dependency.

Checks: native C++ tests with simulated UART and socket transports, ESP32-S3 compilation of all three examples, and all 16,302 discovery combinations in simulation. Real 883M communication, PLC interoperability, network fault recovery and long-duration operation remain unvalidated. This is not production-ready firmware or a complete waveform/FFT capture library.

Manual defaults are 9600 baud, 8N1, slave 1. Match your actual sensor settings and board wiring. Intended model: **883M**, not an unverified 833M variant.

Read the [library guide](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/blob/main/experiments/Wilcoxon883MRTU-beta/README.md) before connecting hardware. `SHA256SUMS.txt` provides the ZIP checksum.
