// Host tests for the real WilcoxonMqtt source against a scripted fake connection.
// They check bytes on the wire and error handling; they do not involve a real broker.
#include <stdio.h>
#include <string.h>
#include "../src/WilcoxonMqtt.cpp"

uint32_t fakeNow = 1000;
static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); failures++; } } while (0)

class FakeClient : public Client {
public:
    uint8_t in[8192], out[8192];
    size_t inLength = 0, inPos = 0, outLength = 0, writeLimit = 100000;
    int connectResult = 1, stops = 0;
    bool open = false, stall = false;
    void feed(const uint8_t* data, size_t n) { memcpy(in + inLength, data, n); inLength += n; }
    int connect(IPAddress, uint16_t) override { open = connectResult == 1; return connectResult; }
    int connect(const char*, uint16_t) override { open = connectResult == 1; return connectResult; }
    size_t write(const uint8_t* data, size_t n) override {
        if (!open || stall) return 0;
        if (n > writeLimit) n = writeLimit;
        memcpy(out + outLength, data, n);
        outLength += n;
        return n;
    }
    int available() override { return open ? int(inLength - inPos) : 0; }
    int read() override { return open && inPos < inLength ? in[inPos++] : -1; }
    void stop() override { open = false; stops++; }
    uint8_t connected() override { return open; }
};

static char lastTopic[64];
static uint8_t lastPayload[64];
static unsigned lastLength = 0, calls = 0;
static void onMessage(char* topic, uint8_t* payload, unsigned int length) {
    strcpy(lastTopic, topic);
    memcpy(lastPayload, payload, length);
    lastLength = length;
    calls++;
}

static const uint8_t ConnAckOk[] = {0x20, 0x02, 0x00, 0x00};

static void connectOk(WilcoxonMqttClient& mqtt, FakeClient& net) {
    net.feed(ConnAckOk, sizeof(ConnAckOk));
    CHECK(mqtt.connect("gw", "u", "p"));
    net.outLength = 0;
}

static void testConnectPacket() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    mqtt.setServer("broker", 1883);
    net.feed(ConnAckOk, sizeof(ConnAckOk));
    CHECK(mqtt.connect("gw", "u", "p"));
    const uint8_t expected[] = {0x10, 20, 0, 4, 'M', 'Q', 'T', 'T', 4, 0xC2, 0, 15,
                                0, 2, 'g', 'w', 0, 1, 'u', 0, 1, 'p'};
    CHECK(net.outLength == sizeof(expected) && !memcmp(net.out, expected, sizeof(expected)));
    CHECK(mqtt.connected() && mqtt.state() == WilcoxonMqttClient::Connected && mqtt.connectCount() == 1);
    // Already connected: no second CONNECT.
    CHECK(mqtt.connect("gw", "u", "p") && net.outLength == sizeof(expected));
}

static void testConnectWithoutCredentials() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    mqtt.setServer(IPAddress(192, 168, 0, 102), 1883);
    net.feed(ConnAckOk, sizeof(ConnAckOk));
    CHECK(mqtt.connect("gw"));
    CHECK(net.out[1] == 14 && net.out[9] == 0x02);
}

static void testConnectFailures() {
    FakeClient refused;
    WilcoxonMqttClient a(refused);
    a.setServer("broker", 1883);
    const uint8_t badCredentials[] = {0x20, 0x02, 0x00, 0x04};
    refused.feed(badCredentials, sizeof(badCredentials));
    CHECK(!a.connect("gw", "u", "wrong") && a.state() == WilcoxonMqttClient::BadCredentials && !refused.open);

    FakeClient silent;
    WilcoxonMqttClient b(silent);
    b.setServer("broker", 1883);
    b.setSocketTimeout(2);
    const uint32_t start = fakeNow;
    CHECK(!b.connect("gw") && b.state() == WilcoxonMqttClient::ConnectionTimeout && !silent.open);
    CHECK(fakeNow - start >= 2000 && fakeNow - start < 2100);

    FakeClient unreachable;
    unreachable.connectResult = 0;
    WilcoxonMqttClient c(unreachable);
    c.setServer("broker", 1883);
    CHECK(!c.connect("gw") && c.state() == WilcoxonMqttClient::ConnectFailed);

    WilcoxonMqttClient noClient;
    CHECK(!noClient.connect("gw") && noClient.state() == WilcoxonMqttClient::ConnectFailed);
}

static void testPublish() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    CHECK(!mqtt.publish("t", "x")); // not connected
    connectOk(mqtt, net);
    CHECK(mqtt.publish("a/b", "hi"));
    const uint8_t plain[] = {0x30, 7, 0, 3, 'a', '/', 'b', 'h', 'i'};
    CHECK(net.outLength == sizeof(plain) && !memcmp(net.out, plain, sizeof(plain)));
    net.outLength = 0;
    // Same overload choice as PubSubClient: (topic, char*, size) selects the retained-flag overload.
    const char* text = "hi";
    const size_t size = 2;
    CHECK(mqtt.publish("a/b", text, size));
    CHECK(net.out[0] == 0x31);
    net.outLength = 0;
    const uint8_t bytes[] = {1, 2, 3};
    CHECK(mqtt.publish("a", bytes, 3));
    CHECK(net.out[0] == 0x30 && net.out[1] == 6 && net.outLength == 8);
}

static void testLargePayloadAndPartialWrites() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    CHECK(mqtt.setBufferSize(64));
    connectOk(mqtt, net);
    static uint8_t payload[3000];
    for (size_t i = 0; i < sizeof(payload); i++) payload[i] = uint8_t(i);
    net.writeLimit = 7; // the network accepts 7 bytes per call
    CHECK(mqtt.publish("wave/1", payload, sizeof(payload)));
    // 3008 remaining = 0xC0 0x17 in the MQTT length encoding.
    CHECK(net.out[0] == 0x30 && net.out[1] == 0xC0 && net.out[2] == 0x17);
    CHECK(net.outLength == 3 + 2 + 6 + sizeof(payload));
    CHECK(!memcmp(net.out + 11, payload, sizeof(payload)));
    // A topic that does not fit the buffer is refused before anything is sent.
    char longTopic[80];
    memset(longTopic, 't', sizeof(longTopic) - 1);
    longTopic[sizeof(longTopic) - 1] = 0;
    const size_t before = net.outLength;
    CHECK(!mqtt.publish(longTopic, "x") && net.outLength == before && mqtt.connected());
}

static void testStalledWrite() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    mqtt.setSocketTimeout(3);
    connectOk(mqtt, net);
    net.stall = true;
    const uint32_t start = fakeNow;
    CHECK(!mqtt.publish("t", "payload"));
    CHECK(mqtt.state() == WilcoxonMqttClient::ConnectionLost && !mqtt.connected() && !net.open);
    CHECK(fakeNow - start >= 3000 && fakeNow - start < 3100);
}

static void testIncoming() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    mqtt.setCallback(onMessage);
    CHECK(mqtt.setBufferSize(32));
    connectOk(mqtt, net);
    CHECK(mqtt.subscribe("cmd", 0));
    const uint8_t sub[] = {0x82, 8, 0, 1, 0, 3, 'c', 'm', 'd', 0};
    CHECK(net.outLength == sizeof(sub) && !memcmp(net.out, sub, sizeof(sub)));
    net.outLength = 0;

    const uint8_t qos0[] = {0x30, 7, 0, 3, 'c', 'm', 'd', 'h', 'i'};
    net.feed(qos0, sizeof(qos0));
    CHECK(mqtt.loop());
    CHECK(calls == 1 && !strcmp(lastTopic, "cmd") && lastLength == 2 && !memcmp(lastPayload, "hi", 2));
    CHECK(net.outLength == 0);

    const uint8_t qos1[] = {0x32, 8, 0, 3, 'c', 'm', 'd', 0x12, 0x34, '!'};
    net.feed(qos1, sizeof(qos1));
    CHECK(mqtt.loop());
    CHECK(calls == 2 && lastLength == 1 && lastPayload[0] == '!');
    const uint8_t ack[] = {0x40, 0x02, 0x12, 0x34};
    CHECK(net.outLength == sizeof(ack) && !memcmp(net.out, ack, sizeof(ack)));

    // A packet larger than the buffer is read and dropped; the next one still arrives.
    uint8_t big[2 + 40];
    big[0] = 0x30; big[1] = 40; big[2] = 0; big[3] = 3; memcpy(big + 4, "cmd", 3);
    memset(big + 7, 'x', sizeof(big) - 7);
    net.feed(big, sizeof(big));
    net.feed(qos0, sizeof(qos0));
    CHECK(mqtt.loop());
    CHECK(mqtt.droppedPackets() == 1 && calls == 3 && lastLength == 2);

    // Broker ping is answered.
    net.outLength = 0;
    const uint8_t ping[] = {0xC0, 0x00};
    net.feed(ping, sizeof(ping));
    CHECK(mqtt.loop() && net.outLength == 2 && net.out[0] == 0xD0 && net.out[1] == 0);

    // A packet cut off mid-way closes the connection instead of reading garbage later.
    const uint8_t cut[] = {0x30, 9, 0, 3, 'c'};
    net.feed(cut, sizeof(cut));
    CHECK(!mqtt.loop() && mqtt.state() == WilcoxonMqttClient::ConnectionLost && calls == 3);
}

static void testKeepAlive() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    mqtt.setKeepAlive(10);
    connectOk(mqtt, net);
    fakeNow += 5000;
    CHECK(mqtt.loop() && net.outLength == 0);
    fakeNow += 5000;
    CHECK(mqtt.loop() && net.outLength == 2 && net.out[0] == 0xC0);
    const uint8_t pong[] = {0xD0, 0x00};
    net.feed(pong, sizeof(pong));
    CHECK(mqtt.loop());
    fakeNow += 10000;
    net.outLength = 0;
    CHECK(mqtt.loop() && net.outLength == 2); // second ping
    fakeNow += 10000;                         // no answer this time
    CHECK(!mqtt.loop() && mqtt.state() == WilcoxonMqttClient::ConnectionTimeout && !net.open);
}

static void testDropAndDisconnect() {
    FakeClient net;
    WilcoxonMqttClient mqtt(net);
    connectOk(mqtt, net);
    net.open = false; // cable pulled / broker closed the socket
    CHECK(!mqtt.connected() && mqtt.state() == WilcoxonMqttClient::ConnectionLost);
    CHECK(!mqtt.loop());

    FakeClient net2;
    WilcoxonMqttClient mqtt2(net2);
    connectOk(mqtt2, net2);
    mqtt2.disconnect();
    CHECK(net2.outLength == 2 && net2.out[0] == 0xE0 && net2.out[1] == 0);
    CHECK(mqtt2.state() == WilcoxonMqttClient::Disconnected && !net2.open);
}

int main() {
    testConnectPacket();
    testConnectWithoutCredentials();
    testConnectFailures();
    testPublish();
    testLargePayloadAndPartialWrites();
    testStalledWrite();
    testIncoming();
    testKeepAlive();
    testDropAndDisconnect();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("PASS: WilcoxonMqtt protocol tests (simulated connection)\n");
    return 0;
}
