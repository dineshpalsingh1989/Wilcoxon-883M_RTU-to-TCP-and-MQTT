# WilcoxonMqtt

Small MQTT 3.1.1 client for Arduino `Client` connections: plain `WiFiClient`, Arduino-Pico lwIP Ethernet (W5500) or a TLS client. It replaces PubSubClient in the Wilcoxon gateways and keeps the PubSubClient calls they use, so firmware switches by changing the type.

**Status: alpha.** Host protocol tests pass against a simulated connection. Broker and hardware validation are pending.

## Use

```cpp
#include <WilcoxonMqtt.h>

WiFiClient net;
WilcoxonMqttClient mqtt(net);

void onMessage(char* topic, uint8_t* payload, unsigned int length) { /* payload is not NUL-terminated */ }

void setup() {
    mqtt.setServer("192.168.1.10", 1883); // the host string is not copied; keep it alive
    mqtt.setBufferSize(1024);              // incoming packets and publish header + topic
    mqtt.setCallback(onMessage);
}

void loop() {
    if (!mqtt.connected() && mqtt.connect("gateway-1", "user", "password")) mqtt.subscribe("883M/command");
    mqtt.loop();
    mqtt.publish("883M/status", "{\"ok\":true}");
}
```

| Call | Notes |
| --- | --- |
| `setServer(host or IPAddress, port)`, `setClient(client)`, `setCallback(fn)` | `fn` is a plain function `void(char*, uint8_t*, unsigned int)`. |
| `setBufferSize(bytes)`, `setKeepAlive(s)`, `setSocketTimeout(s)` | Defaults: 256 bytes, 15 s, 15 s. The buffer is allocated once, not grown. |
| `connect(id[, user, pass])` | Clean session. User and password are sent when non-null, even when empty. Blocks for at most the socket timeout while waiting for the broker. |
| `loop()` | Sends keep-alive pings and handles up to four waiting packets per call. |
| `publish(topic, payload[, length][, retained])` | QoS 0. Overloads match PubSubClient, including `publish(topic, const char*, size)` selecting the retained-flag overload. |
| `subscribe(topic[, qos])`, `unsubscribe(topic)` | QoS 0 or 1. Incoming QoS 1 messages are acknowledged after the callback. |
| `connected()`, `disconnect()`, `state()` | `state()` uses PubSubClient's numbers: -4 timeout, -3 lost, -2 connect failed, -1 disconnected, 0 connected, 1-5 broker refusal codes. |
| `connectCount()`, `droppedPackets()` | Diagnostics: successful connections, and incoming packets dropped for being larger than the buffer. |

## Differences from PubSubClient

- Payloads are written straight from your memory after the header and topic, so a publish may be larger than the buffer and is never copied. Only the topic must fit in the buffer.
- Every write and read has a time limit. A partial write or a packet cut off mid-way closes the connection (`state()` -3) rather than leaving the broker stream out of step.
- Not supported: QoS 1/2 publishing, last-will messages, persistent sessions, `publish_P` and streaming `beginPublish()`.
- Not thread-safe: use one task for all calls on a client.

## Checks

```sh
python tests/run_native.py
python tests/run_native.py --broker 192.168.1.10:1883 [--user U --password P]
```

The tests build the real source on the host with a scripted fake connection and check the bytes sent, broker refusals, timeouts, partial and stalled writes, keep-alive, incoming QoS 0/1 messages, oversized packets and dropped connections. They need a host C++ compiler: `g++`, `clang++`, the workspace's `.tools/native-compiler` Zig, or `CXX`. Passing them does not validate a real broker or network.

With `--broker`, the real source also runs on your computer against that broker over plain TCP. It connects, subscribes, publishes and reads back a short message and a 20,000-byte message, stays connected through keep-alive pings, and disconnects. Messages are not retained and use `wilcoxonmqtt/selftest/<id>/`. This checks the protocol against a real broker; it does not test the IOC, W5500 or lwIP.
