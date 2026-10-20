# Wilcoxon883MRTU - beta library

**Version 0.1.0-beta.4. ESP32 + Wilcoxon 883M over RS485 Modbus RTU, with an optional Modbus TCP server for PLCs.**

This is a standalone Arduino/PlatformIO library, extracted and refined from the separate Modbus lab. It has two independent parts: `Wilcoxon883MRTU.h` reads the sensor, and the optional `Wilcoxon883MTCP.h` serves data to PLCs (see [Modbus TCP server](#modbus-tcp-server-for-plcs)). It has no MQTT, web dashboard or third-party Modbus dependency. It is not affiliated with or certified by Wilcoxon. The sensor in this repository is the **883M**; support for a different model such as an 833M has not been established.

The gateway firmware in this repository is built with this library (beta, starting with gateway firmware v4.10.0-beta.1). Stable gateway releases up to v4.9.0 still use the previous third-party drivers. Real sensor and PLC validation is still outstanding.

## Download the beta

[GitHub beta pre-release](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases/tag/wilcoxon883mrtu-v0.1.0-beta.3) - download **`Wilcoxon883MRTU-0.1.0-beta.3.zip`** from its Assets section. That published release is RTU only; the TCP server arrives with the beta.4 release. GitHub's automatic source-code ZIP contains the whole repository; use the named library ZIP for Arduino installation.

**Does it come with my ESP32 upload?**

| Action | Is this beta library used? |
| --- | --- |
| Build or upload the root gateway project from `main` | Yes; the gateway compiles this library from `experiments/Wilcoxon883MRTU-beta` |
| Download or install the beta ZIP | No upload occurs; this only makes the library available |
| Compile and upload an example, or a sketch that includes `Wilcoxon883MRTU.h` or `Wilcoxon883MTCP.h` | Yes; the beta code is compiled into that sketch |
| Use the gateway GitHub firmware updater | Only through a gateway firmware release built with it; this source ZIP itself is not a gateway update |

Uploading an example replaces the application on the selected ESP32. Use a spare bench board or keep your gateway firmware available to restore. The beta release does not contain a flashable gateway binary.

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

The bundled **883M User Manual Rev.1.0 (2024-12-06)** lists defaults of **9600 baud, 8 data bits, no parity, 1 stop bit, slave ID 1** (section 6, PDF page 14). A sensor already configured for another baud/parity/ID must use matching values. `begin()` changes the ESP32 UART, not sensor settings. Supported UART formats are 8N1, 8N2, 8E1, 8O1, 8E2 and 8O2; supported baud rates follow the manual's 1200-921600 list.

`readMetrics()` reads three groups from function 04, based on section 7 (PDF pages 16-18):

| Field | Registers | Conversion |
| --- | --- | --- |
| `accelerationRmsG[0..2]` | `0x0000..0x0002` | Unsigned raw value / 100, g, 2 Hz-1 kHz RMS |
| `velocityRmsMmS[0..2]` | `0x0020..0x0022` | Unsigned raw value / 10, mm/s, 10 Hz-1 kHz RMS |
| `temperatureC` | `0x0050` | Signed 16-bit raw value / 10, degrees C |

Indices 0, 1 and 2 correspond to X, Y and Z. The method preserves the caller's previous output if any of the three reads fails. These are sequential sensor reads, not a simultaneous atomic sensor snapshot.

## Plug-and-play discovery for non-default settings

Open **File > Examples > Wilcoxon883MRTU > AutoDiscover**. Connect a single 883M to an isolated RS485 link with the ESP32 as the only master, then upload the example to a bench board. The library searches for the sensor and leaves the ESP32 UART configured for the confirmed settings so measurement reads can follow immediately.

```cpp
Wilcoxon883MRTU sensor(Serial1);

void setup() {
    Serial.begin(115200);
    sensor.startDiscovery(18, 17, 14);
}

void loop() {
    auto state = sensor.pollDiscovery();
    if (state == Wilcoxon883MRTU::DiscoveryState::Found) {
        const auto& found = sensor.discoveryResult();
        // found.settings.baud, .unit, .mode: confirmed sensor settings.
        // found.identity.vendor, .model, .product and .revision: identification.
        Wilcoxon883MRTU::Metrics metrics;
        if (sensor.readMetrics(metrics) == Wilcoxon883MRTU::Result::Ok) {
            Serial.println(metrics.temperatureC);
        }
        delay(2000);
    }
    delay(1);
}
```

`AutoDiscover.ino` also shows progress, timeout handling and cancellation by typing `x` in Serial Monitor. `pollDiscovery()` is cooperative but not instant/nonblocking: it performs one candidate per call, potentially several bounded requests for a responding device. Cancellation and the global time limit are checked between candidate probes; they can overshoot by the duration of the active probe. Give the library one UART/task owner and keep your application watchdog servicing appropriate to those bounded calls.

### What is searched and verified

Based on your supplied **Wilcoxon 883M manual 97052 Rev.A (09/26)**, sections 4.3.1 (page 18) and 4.7 (pages 38-41):

1. Search the eleven manual-listed baud rates, six combinations of None/Even/Odd parity with one/two stop bits, and IDs 1-247. Eight data bits are fixed by the manual.
2. Read holding registers `0x0000..0x0004` for the reported baud code, data bits, parity, stop bits and slave ID. No sensor setting is written.
3. Reconfigure only the ESP32 UART to the reported settings and read the table again. This avoids reporting a compatible stop-bit probe as the sensor's actual configuration.
4. Read device identification using function `0x2B`, MEI `0x0E`, basic/regular requests with bounded pagination. By default, require vendor **Wilcoxon** and model **883M** before returning `Found`.

There are **16,302 possible tuples**. The scanner tries common 9600/115200 rates and 8N1 early, first across the first ID's profiles and then across remaining IDs. The response timeout is extended when needed for the maximum identity frame at the detected baud, including 1200 baud. The default overall budget is **two minutes**, not a guarantee to search every tuple. A complete exhaustive search can take **over an hour**. Silence, slow firmware, wrong wiring, incorrect power or duplicate addresses cannot be solved by scanning. A time-limit result means the search was incomplete, not proof that no sensor exists.

### Narrow, extend or cancel a search

```cpp
Wilcoxon883MRTU::DiscoveryOptions options;
options.baud = 115200;                 // 0 = all manual-listed rates.
options.firstUnit = 1;
options.lastUnit = 16;
options.mode = Wilcoxon883MRTU::SerialMode::Any;
options.responseTimeoutMs = 250;       // Floor: 150..5000 ms; extended for slow-baud response wire time.
options.maxDurationMs = 120000;        // 0 = allow all finite candidates to be tried.
options.requireIdentity = true;       // Default: verify Wilcoxon + 883M.
sensor.startDiscovery(18, 17, 14, options);
// From your loop, between poll calls:
// sensor.cancelDiscovery();
```

Progress provides `state`, `attempts`, `totalCandidates` and the last `candidate`. Terminal states are `Found`, `Exhausted`, `TimedOut` and `Cancelled`. Read `discoveryResult()` only after `Found`. During a scan, ordinary reads/writes return `Busy`, and `begin()` returns false. Call `cancelDiscovery()` before manual reconfiguration. After an unsuccessful/cancelled search, the UART may remain on its last attempted settings; the library does not restore or persist previous settings.

Older sensor firmware may not support regular identification/model objects. Set `requireIdentity=false` only if you deliberately want a **settings-confirmed RTU candidate** without model confirmation; inspect `identityVerified` before calling it an 883M. A device explicitly identifying itself as another model is rejected even in this fallback mode. The default never guesses model identity from vibration values alone.

The search emits function 03 and 43 reads only. It does not issue broadcasts, reset the sensor, change its address/baud, or trigger captures. Do not run a search on a live shared bus with another master. Physical RS485 discovery has not been tested on a real sensor yet, so this remains beta rather than guaranteed plug-and-play.

## Low-level API

| Method | Purpose |
| --- | --- |
| `readInputRegisters(address, count, values, capacity, timeoutMs)` | Function 04, 1-125 raw register words |
| `readHoldingRegisters(address, count, values, capacity, timeoutMs)` | Function 03, 1-125 raw register words |
| `writeSingleRegister(address, value, timeoutMs)` | Function 06; validates the sensor's echo |
| `readMetrics(metrics, timeoutPerRequestMs)` | Read and convert acceleration RMS, velocity RMS and temperature |
| `readIdentity(identity, timeoutMs)` | Read bounded basic/regular device identification |
| `startDiscovery(...)`, `pollDiscovery()`, `cancelDiscovery()` | Search, advance or cancel read-only discovery |
| `exceptionCode()` | Sensor exception code when the last result was `Exception` |
| `resultText(result)` | Human-readable result description |

`values` must point to at least `capacity` words and `capacity` must be at least `count`. Only complete, validated responses update read buffers. Return values distinguish invalid arguments, timeout, malformed frame, CRC failure, sensor exception and transport failure. CRC validation covers the entire received frame, including trailing bytes.

Function 06 can change sensor configuration or trigger captures. The example only reads. Consult section 8 of the manual before writing any register. Multiple-register writes, automatic retries, complete waveform/spectrum capture workflows and sensor setting management are not included in this beta. Read-only device identity (43/14) and serial discovery are included. Raw reads support at most 125 registers per request; actual sensor limits still need bench verification.

## Modbus TCP server for PLCs

`Wilcoxon883MTCP` lets PLCs and SCADA read data from the ESP32 over Wi-Fi. It is the output side: your sketch reads the sensor over RTU, stores the values in arrays, and the server answers PLC requests from those arrays at **its own addresses**. PLC requests never wait for RS485.

```
883M sensor --RS485 RTU--> ESP32 arrays --Modbus TCP--> PLC / SCADA
```

Open **File > Examples > Wilcoxon883MRTU > RtuToTcp** for a complete sketch. It uses the gateway's holding-register addresses for the values `readMetrics()` provides:

| Holding register | Value | Scaling |
| --- | --- | --- |
| 0 / 20 / 40 | Acceleration RMS X / Y / Z | g x100 |
| 4 / 24 / 44 | Velocity RMS X / Y / Z | mm/s x10 |
| 60 | Temperature | degrees C x10, signed |
| 99 | Last RTU result code (example only) | 0 = OK |

```cpp
#include <Wilcoxon883MTCP.h>

uint16_t registers[100];
int16_t waveform[13334];
Wilcoxon883MTCP::RegisterBlocks<2> bank;
Wilcoxon883MTCP plcServer(bank);

void setup() {
    // ... connect Wi-Fi first ...
    bank.addHolding(0, registers, 100);          // Read-only for PLCs.
    bank.addHolding(1000, waveform, 13334);      // int16_t arrays are served as raw words.
    plcServer.begin(502, 4);                     // Port, max clients (1-8), unit ID (0 = any).
}

void loop() {
    plcServer.poll();                            // Call often; never blocks on a slow client.
}
```

Supported: functions 03 (read holding), 04 (read input), 06 (write single) and 16 (write multiple); up to 125 registers per read and 123 per write; standard exception replies (illegal function, address and value, plus busy when a lock cannot be taken). A request must fit inside one block. Writes are refused unless a block is added with `writable = true`; refused writes leave every register unchanged.

| Method | Purpose |
| --- | --- |
| `begin(port, maxClients, unitId)` | Start listening after the network is up. Unit ID 0 accepts any ID, like the current gateway |
| `poll()` | Accept clients and answer at most one request per client per call |
| `end()` | Close all clients and stop listening |
| `setTimeouts({idleMs, frameMs})` | Close silent clients (default 60 s, 0 disables) and stalled partial requests (default 5 s) |
| `clientCount()`, `stats()` | Connected clients; counters for requests, exceptions, ignored, malformed, timed-out and rejected connections |
| `RegisterBlocks<N>::addHolding(start, data, count, writable)` | Serve an existing `uint16_t`/`int16_t` array as holding registers |
| `RegisterBlocks<N>::addInput(start, data, count)` | Serve an array as read-only input registers |
| `RegisterBlocks<N>::setLock(lock, unlock, context)` | Wrap each request in your lock, e.g. a FreeRTOS mutex |
| `RegisterBlocks<N>::onWrite(callback, context)` | Notified after a PLC write, outside the lock |

`RegisterBlocks` reads your arrays directly instead of keeping a second copy of every register. If another task updates the arrays, set a lock so a PLC reply never mixes old and new values:

```cpp
SemaphoreHandle_t dataMutex = xSemaphoreCreateMutex();
bank.setLock([](void* m) { return xSemaphoreTake((SemaphoreHandle_t)m, pdMS_TO_TICKS(50)) == pdTRUE; },
             [](void* m) { xSemaphoreGive((SemaphoreHandle_t)m); }, dataMutex);
```

For other storage, derive from `Wilcoxon883MTCP::RegisterBank` and override `readRegisters()` and/or `writeRegisters()`; unimplemented operations return an illegal-address exception.

Limits: one task should own `poll()`. A full server refuses new connections rather than dropping existing ones. A wrong unit ID (when filtered) is ignored without reply; a malformed MBAP header closes that connection. There is no authentication or encryption, as with standard Modbus TCP; keep the port on a trusted network. This is a register server, not a transparent TCP-to-RTU bridge. PLC interoperability has not been bench-tested yet.

## Timing and ownership

Use one task/owner per instance and UART. The library does not create FreeRTOS tasks, configure a watchdog, allocate capture buffers or manage shared locks. Calls are synchronous; the receive wait yields to the scheduler. Do not call them from an interrupt. The timeout parameter is bounded to 1-60000 ms and applies separately to obtaining a silent bus and receiving a response; `readMetrics()` performs three transactions. UART write/flush uses the ESP32 driver and is not covered by that receive timeout.

The client waits for frame-end silence and validates CRC, unit, function, lengths and write echoes. It does not claim accurate inter-character timing validation from UART dequeue timestamps. Late replies after a timeout, electrical noise, transceiver direction timing and high-baud operation require hardware testing. No automatic retry is performed.

## Tests and beta status

From the repository root:

```sh
python experiments/Wilcoxon883MRTU-beta/tests/run_native.py
```

This runs the actual portable RTU and TCP implementations against simulated UART and socket transports. It needs Python 3 and a native C++11 compiler (`g++`, `clang++`, or `CXX` set to an executable). This workspace can use its ignored local Zig compiler automatically.

Covered: known CRC vector, 125-register replies including values after index 63, invalid ranges and capacities, transmit/receive direction order, partial/corrupt/oversized responses, wrong slave, exception replies, response timeout, UART write failure, timer wrap, unsigned vibration scaling, negative temperature and failed snapshot preservation.

TCP tests cover 125-register reads, input/holding address spaces, signed arrays, every exception path, FC06/FC16 writes with all-or-nothing rejection, unit filtering, byte-by-byte framing, pipelined requests, partial and would-block sends, idle/frame timeouts with millis() wrap, malformed headers, disconnects, send errors and lock/write callbacks.

BasicRead, AutoDiscover and RtuToTcp are compiled for the configured ESP32-S3 board. Native discovery tests also cover every candidate exactly once, reported-setting aliases, wrong models, legacy identity fallback, pagination, global deadlines and cancellation. No sensor, UART timing, PLC interoperability, real network behavior, physical fault recovery or long-duration operation has been validated. Keep this at beta until bench tests establish those behaviors.

Source reference: the [bundled 883M manual](../../883M-User-Manual-Rev.1.0_20241206-EN.pdf). When copying the library out of this repository, obtain that manual separately; repository-relative documentation links will no longer resolve.

License: [MIT](LICENSE).

## Maintainer checks and packaging

```sh
python experiments/Wilcoxon883MRTU-beta/tests/run_native.py
pio run --project-dir experiments/Wilcoxon883MRTU-beta/tests/esp32_compile
pio run --project-dir experiments/Wilcoxon883MRTU-beta/tests/esp32_discovery
pio run --project-dir experiments/Wilcoxon883MRTU-beta/tests/esp32_tcp
python experiments/Wilcoxon883MRTU-beta/tests/package_library.py --output .tools/artifacts
```

The library release workflow runs tests, compiles all three examples, packages the source ZIP and publishes a **pre-release** only for tags matching `wilcoxon883mrtu-v<VERSION>`. The tag must match the manifest's beta version. This separate tag prefix avoids the gateway firmware release workflow. The release is explicitly not marked Latest.
