#pragma once
#include <Arduino.h>
#include <Client.h>
#include <IPAddress.h>
#include "WilcoxonMqttVersion.h"

// MQTT 3.1.1 client (QoS 0 publish, QoS 0/1 subscribe) over any Arduino Client:
// plain WiFiClient / lwIP Ethernet or a TLS client. The calls match the PubSubClient
// subset the gateways use, so firmware switches by changing the type only.
// Not thread-safe: call every method from the task that owns the connection.
//
// Differences from PubSubClient, on purpose:
// - The packet buffer holds incoming packets and the publish header + topic only;
//   payloads are written straight from the caller's memory, so they may be larger
//   than the buffer and are never copied.
// - Every write and read has a time limit (setSocketTimeout); a partial write or a
//   packet cut off mid-way closes the connection (state ConnectionLost) instead of
//   leaving the broker stream out of step.
class WilcoxonMqttClient {
public:
    typedef void (*Callback)(char* topic, uint8_t* payload, unsigned int length);
    // Same numbers as PubSubClient's MQTT_* states; 1-5 are the broker's CONNACK refusal codes.
    enum : int {
        ConnectionTimeout = -4, ConnectionLost = -3, ConnectFailed = -2, Disconnected = -1, Connected = 0,
        BadProtocol = 1, BadClientId = 2, Unavailable = 3, BadCredentials = 4, Unauthorized = 5
    };

    WilcoxonMqttClient() {}
    explicit WilcoxonMqttClient(Client& client) : client_(&client) {}
    ~WilcoxonMqttClient();
    WilcoxonMqttClient(const WilcoxonMqttClient&) = delete;
    WilcoxonMqttClient& operator=(const WilcoxonMqttClient&) = delete;

    WilcoxonMqttClient& setClient(Client& client) { client_ = &client; return *this; }
    // host must stay valid while the client is used (it is not copied).
    WilcoxonMqttClient& setServer(const char* host, uint16_t port) { host_ = host; port_ = port; return *this; }
    WilcoxonMqttClient& setServer(IPAddress ip, uint16_t port) { host_ = nullptr; ip_ = ip; port_ = port; return *this; }
    WilcoxonMqttClient& setCallback(Callback callback) { callback_ = callback; return *this; }
    WilcoxonMqttClient& setKeepAlive(uint16_t seconds) { keepAlive_ = seconds; return *this; }
    WilcoxonMqttClient& setSocketTimeout(uint16_t seconds) { socketTimeout_ = seconds; return *this; }
    // Allocated once here (default 256 bytes on first use); false keeps the old buffer.
    bool setBufferSize(uint16_t size);
    uint16_t getBufferSize() const { return bufferSize_; }

    // user and pass are sent when non-null, even when empty, as PubSubClient does.
    bool connect(const char* id) { return connect(id, nullptr, nullptr); }
    bool connect(const char* id, const char* user, const char* pass);
    void disconnect();
    bool connected();
    // Sends keep-alive pings and handles up to four waiting packets; false when not connected.
    bool loop();

    bool publish(const char* topic, const char* payload) { return publish(topic, payload, false); }
    bool publish(const char* topic, const char* payload, bool retained);
    bool publish(const char* topic, const uint8_t* payload, unsigned int length) { return publish(topic, payload, length, false); }
    bool publish(const char* topic, const uint8_t* payload, unsigned int length, bool retained);
    bool subscribe(const char* topic, uint8_t qos = 0);
    bool unsubscribe(const char* topic);

    int state() const { return state_; }
    // Diagnostics for status pages.
    uint32_t connectCount() const { return connects_; }
    uint32_t droppedPackets() const { return dropped_; }

private:
    static const uint8_t MaxHeader = 5; // fixed header byte + up to 4 length bytes
    bool ensureBuffer();
    size_t putHeader(uint8_t type, uint32_t remaining);
    bool writeAll(const uint8_t* data, size_t length);
    bool sendSimple(uint8_t type, uint16_t id, bool withId);
    bool waitAvailable();
    bool readByte(uint8_t& value);
    long readPacket();
    void handlePublish(size_t length);
    void closeWith(int state);

    Client* client_ = nullptr;
    const char* host_ = nullptr;
    IPAddress ip_;
    uint16_t port_ = 1883;
    Callback callback_ = nullptr;
    uint8_t* buffer_ = nullptr;
    uint16_t bufferSize_ = 0;
    uint16_t keepAlive_ = 15, socketTimeout_ = 15, nextId_ = 0;
    uint32_t lastIn_ = 0, lastOut_ = 0, connects_ = 0, dropped_ = 0;
    bool pingOutstanding_ = false, connected_ = false;
    int state_ = Disconnected;
};
