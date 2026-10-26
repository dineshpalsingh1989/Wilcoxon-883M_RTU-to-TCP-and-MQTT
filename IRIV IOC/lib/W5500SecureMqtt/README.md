# W5500SecureMqtt

Experimental 0.1.0-alpha.3 library for certificate-validated MQTT over Arduino-Pico's lwIP Ethernet stack. Initially targets IRIV IOC (RP2350 + W5500); other W5500 boards are unverified. This is an original integration layer over Arduino-Pico BearSSL and the sibling [WilcoxonMqtt](../WilcoxonMqtt/README.md) client, not a replacement cryptographic implementation.

## Use

Copy this folder into your PlatformIO project's `lib/`, or an Arduino sketchbook library directory. Also copy the sibling WilcoxonMqtt library. Use the Arduino-Pico core and `W5500lwIP`, rather than the separate Arduino Ethernet hardware-socket stack.

The [IRIV example](examples/IRIVSecurePublish/main.cpp) also requires the sibling [IRIVIOCHardware](../IRIVIOCHardware/README.md) library and ArduinoJson 6. Configure your broker's DNS hostname, public CA PEM, unique client ID and any credentials locally. The example deliberately does not connect with its empty CA placeholder. It publishes an explicitly synthetic test message, not sensor readings.

Keep hostname, client ID, credential strings and the `BearSSL::X509List` alive for the entire connection lifetime. Call `begin()` from `setup()` and `tick(ethernet.connected())` repeatedly from `loop()`. Check `publish()` and `subscribe()` return values. Resubscribe when transitioning into `Connected` after reconnecting. Call `stop()` before releasing configuration objects.

TLS 1.2 is required. The library checks clock readiness, configures trust anchors, connects by hostname and never falls back to unencrypted MQTT. The default begin() API verifies certificates. The separate beginInsecure() API explicitly skips certificate authentication and clock validation while keeping TLS 1.2 encryption. This optional mode does not authenticate the broker and requires no CA. It relies on BearSSL for certificate-chain, hostname and validity checks. The application must establish trustworthy current time; the readiness threshold alone does not prove time accuracy. Brokers requiring client certificates are not supported by this initial API.

Connection attempts are synchronous and can take seconds. Retries are spaced ten seconds after an attempt completes, doubling after each failure up to 60 seconds and returning to ten seconds after a successful connection. MQTT packets have a 1024-byte default buffer (the optional final begin argument selects a different size; the gateway uses 4096 bytes), for incoming packets and the publish header and topic; payloads are written directly and may be larger. Publishing uses QoS 0 and is not proof of subscriber delivery. No offline queue is provided. The MQTT socket timeout (CONNACK wait) is 5 seconds; this is not a strict upper bound on total handshake duration.

## Task restrictions

All networking must run on core 0 from Arduino's setup/loop task, including when FreeRTOS is enabled. Do not call from custom tasks, interrupts or concurrent callbacks. The IRIV gateway beta now uses Arduino loop with its own stack adaptation; see its secure MQTT beta guide. The library itself does not change task stacks. Use protocolClient() only for existing publish/subscribe APIs; tick() owns connection setup and verification.

References: [Arduino-Pico wired Ethernet restrictions](https://arduino-pico.readthedocs.io/en/stable/ethernet.html), [BearSSL certificate validation](https://arduino-pico.readthedocs.io/en/stable/bearssl-client-secure-class.html).

## Verification before release

Build the example from this directory with `pio run -d tests/iriv_compile`. This compiles both new libraries with FreeRTOS enabled. Compilation does not validate hardware behavior.

On real hardware, record core/platform versions, broker configuration and results for:

- Valid CA, matching DNS hostname and correct time: connect, publish and subscribe.
- Wrong CA, expired certificate and mismatched hostname: reject the connection.
- Missing time: stay in `WaitingTime`; recover when time is set.
- Wrong MQTT credentials: report failure without disclosing credentials.
- Cable removal, broker restart and DHCP recovery: reconnect and resubscribe.
- Long reconnect runs: observe heap, responsiveness and watchdog behavior.
- Oversized payload: return failure without corrupting subsequent messages.

Hardware validation is pending. Do not advertise universal W5500 support or a proven fix for the existing gateway yet. Preserve dependency licenses when publishing.
