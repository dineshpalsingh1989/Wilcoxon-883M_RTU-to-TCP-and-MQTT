# Wilcoxon883MRTU - beta library

**Version 0.1.0-beta.1. ESP32 + Wilcoxon 883M over RS485 Modbus RTU only.**

This is a standalone Arduino/PlatformIO library, extracted and refined from the separate Modbus lab. It has no TCP, Wi-Fi, MQTT, web dashboard or third-party Modbus dependency. It is not affiliated with or certified by Wilcoxon. The sensor in this repository is the **883M**; support for a different model such as an 833M has not been established.

The production gateway is unchanged. Do not treat this beta as a validated replacement for its existing driver.

## Download the beta

[GitHub beta pre-release](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases/tag/wilcoxon883mrtu-v0.1.0-beta.1) - download **`Wilcoxon883MRTU-0.1.0-beta.1.zip`** from its Assets section. GitHub's automatic source-code ZIP contains the whole repository; use the named library ZIP for Arduino installation.

**Does it come with my ESP32 upload?**

| Action | Is this beta library used? |
| --- | --- |
| Upload the existing root gateway project | No; it still uses the existing Modbus libraries |
| Download or install the beta ZIP | No upload occurs; this only makes the library available |
| Compile and upload BasicRead, or a sketch that includes and uses `Wilcoxon883MRTU.h` | Yes; the beta code is compiled into that sketch |
| Use the gateway's GitHub firmware updater | No; this source-only beta is not a gateway firmware update |

Uploading BasicRead replaces the application on the selected ESP32. Use a spare bench board or keep your gateway firmware available to restore. The beta release does not contain a flashable gateway binary.

## Install

- **Arduino IDE:** add the packaged ZIP using Sketch > Include Library > Add .ZIP Library, or copy this directory into your sketchbook's `libraries/Wilcoxon883MRTU` directory. Open File > Examples > Wilcoxon883MRTU > BasicRead.
- **PlatformIO:** copy this directory into a separate test project's `lib/Wilcoxon883MRTU` directory. The package includes `library.json` and `library.properties`; only the `src/` directory is compiled as library code.

An ESP32 Arduino core is required. The example has been compiled for ESP32-S3; other ESP32 boards and Arduino core versions require their own validation.

## Basic usage

```cpp
#include <Wilcoxon883MRTU.h>

Wilcoxon883MRTU sensor(Serial1);

void setup() {
    Serial.begin(115200);
    // RX, TX, shared DE + /RE, baud, slave ID, UART format.
    if (!sensor.begin(18, 17, 14, 9600, 1, SERIAL_8N1)) {
        Serial.println("UART configuration failed");
    }
}

void loop() {
    Wilcoxon883MRTU::Metrics readings;
    auto status = sensor.readMetrics(readings);
    if (status == Wilcoxon883MRTU::Result::Ok) {
        Serial.printf("X velocity RMS: %.2f mm/s; temperature: %.1f C\n",
            readings.velocityRmsMmS[0], readings.temperatureC);
    } else {
        Serial.println(Wilcoxon883MRTU::resultText(status));
    }
    delay(2000);
}
```

Use your actual board GPIOs and a 3.3 V logic-compatible RS485 transceiver. The example GPIOs match the current gateway. Power the sensor separately according to its manual; no direct RS485-to-GPIO connection. See the repository's [wiring guide](../../docs/wiring.md).

## Serial settings and sensor helpers

The bundled **883M User Manual Rev.1.0 (2024-12-06)** lists defaults of **9600 baud, 8 data bits, no parity, 1 stop bit, slave ID 1** (section 6, PDF page 14). A sensor already configured for another baud/parity/ID must use matching values. `begin()` changes the ESP32 UART, not sensor settings. Supported UART formats are 8N1, 8N2, 8E1 and 8O1; supported baud rates follow the manual's 1200-921600 list.

`readMetrics()` reads three groups from function 04, based on section 7 (PDF pages 16-18):

| Field | Registers | Conversion |
| --- | --- | --- |
| `accelerationRmsG[0..2]` | `0x0000..0x0002` | Unsigned raw value / 100, g, 2 Hz-1 kHz RMS |
| `velocityRmsMmS[0..2]` | `0x0020..0x0022` | Unsigned raw value / 10, mm/s, 10 Hz-1 kHz RMS |
| `temperatureC` | `0x0050` | Signed 16-bit raw value / 10, degrees C |

Indices 0, 1 and 2 correspond to X, Y and Z. The method preserves the caller's previous output if any of the three reads fails. These are sequential sensor reads, not a simultaneous atomic sensor snapshot.

## Low-level API

| Method | Purpose |
| --- | --- |
| `readInputRegisters(address, count, values, capacity, timeoutMs)` | Function 04, 1-125 raw register words |
| `readHoldingRegisters(address, count, values, capacity, timeoutMs)` | Function 03, 1-125 raw register words |
| `writeSingleRegister(address, value, timeoutMs)` | Function 06; validates the sensor's echo |
| `readMetrics(metrics, timeoutPerRequestMs)` | Read and convert acceleration RMS, velocity RMS and temperature |
| `exceptionCode()` | Sensor exception code when the last result was `Exception` |
| `resultText(result)` | Human-readable result description |

`values` must point to at least `capacity` words and `capacity` must be at least `count`. Only complete, validated responses update read buffers. Return values distinguish invalid arguments, timeout, malformed frame, CRC failure, sensor exception and transport failure. CRC validation covers the entire received frame, including trailing bytes.

Function 06 can change sensor configuration or trigger captures. The example only reads. Consult section 8 of the manual before writing any register. Identity (43/14), multiple-register writes, automatic retries, complete waveform/spectrum capture workflows and sensor setting management are not included in this beta. Raw reads support at most 125 registers per request; actual sensor limits still need bench verification.

## Timing and ownership

Use one task/owner per instance and UART. The library does not create FreeRTOS tasks, configure a watchdog, allocate capture buffers or manage shared locks. Calls are synchronous; the receive wait yields to the scheduler. Do not call them from an interrupt. The timeout parameter is bounded to 1-60000 ms and applies separately to obtaining a silent bus and receiving a response; `readMetrics()` performs three transactions. UART write/flush uses the ESP32 driver and is not covered by that receive timeout.

The client waits for frame-end silence and validates CRC, unit, function, lengths and write echoes. It does not claim accurate inter-character timing validation from UART dequeue timestamps. Late replies after a timeout, electrical noise, transceiver direction timing and high-baud operation require hardware testing. No automatic retry is performed.

## Tests and beta status

From the repository root:

```sh
python experiments/Wilcoxon883MRTU-beta/tests/run_native.py
```

This runs the actual portable RTU implementation against a simulated transport. It needs Python 3 and a native C++11 compiler (`g++`, `clang++`, or `CXX` set to an executable). This workspace can use its ignored local Zig compiler automatically.

Covered: known CRC vector, 125-register replies including values after index 63, invalid ranges and capacities, transmit/receive direction order, partial/corrupt/oversized responses, wrong slave, exception replies, response timeout, UART write failure, timer wrap, unsigned vibration scaling, negative temperature and failed snapshot preservation.

The BasicRead example and library are compiled for the configured ESP32-S3 board. No sensor, UART timing, physical fault recovery or long-duration operation has been validated. Keep this at beta until bench tests establish those behaviors.

Source reference: the [bundled 883M manual](../../883M-User-Manual-Rev.1.0_20241206-EN.pdf). When copying the library out of this repository, obtain that manual separately; repository-relative documentation links will no longer resolve.

License: [MIT](LICENSE).

## Maintainer checks and packaging

```sh
python experiments/Wilcoxon883MRTU-beta/tests/run_native.py
pio run --project-dir experiments/Wilcoxon883MRTU-beta/tests/esp32_compile
python experiments/Wilcoxon883MRTU-beta/tests/package_library.py --output .tools/artifacts
```

The library release workflow runs tests, compiles BasicRead, packages the source ZIP and publishes a **pre-release** only for tags matching `wilcoxon883mrtu-v<VERSION>`. The tag must match the manifest's beta version. This separate tag prefix avoids the gateway firmware release workflow. The release is explicitly not marked Latest.
