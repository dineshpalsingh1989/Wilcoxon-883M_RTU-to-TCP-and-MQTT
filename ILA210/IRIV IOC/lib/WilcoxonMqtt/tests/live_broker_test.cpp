// Runs the real WilcoxonMqtt source on this computer against a real broker over plain TCP.
// It publishes non-retained messages under wilcoxonmqtt/selftest/<id>/ and reads them back.
// This checks the protocol against a broker; it does not test the IOC, W5500 or lwIP.
// Usage: live_broker_test <host> [port] [user] [password]
#define WILCOXON_MQTT_LIVE 1
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Socket;
static const Socket NoSocket = INVALID_SOCKET;
static void closeSocket(Socket s) { closesocket(s); }
static bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
static int pending(Socket s) { u_long n = 0; return ioctlsocket(s, FIONREAD, &n) == 0 ? int(n) : -1; }
static void setNonBlocking(Socket s) { u_long on = 1; ioctlsocket(s, FIONBIO, &on); }
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int Socket;
static const Socket NoSocket = -1;
static void closeSocket(Socket s) { close(s); }
static bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
static int pending(Socket s) { int n = 0; return ioctl(s, FIONREAD, &n) == 0 ? n : -1; }
static void setNonBlocking(Socket s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK); }
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/WilcoxonMqtt.cpp"

class SocketClient : public Client {
public:
    int connect(IPAddress, uint16_t) override { return 0; }
    int connect(const char* host, uint16_t port) override {
        stop();
        char service[8];
        snprintf(service, sizeof(service), "%u", port);
        addrinfo hints = {}, *found = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, service, &hints, &found) || !found) return 0;
        socket_ = socket(found->ai_family, found->ai_socktype, found->ai_protocol);
        const bool ok = socket_ != NoSocket && ::connect(socket_, found->ai_addr, int(found->ai_addrlen)) == 0;
        freeaddrinfo(found);
        if (!ok) { stop(); return 0; }
        setNonBlocking(socket_);
        open_ = true;
        return 1;
    }
    size_t write(const uint8_t* data, size_t n) override {
        if (!open_) return 0;
        const int sent = send(socket_, reinterpret_cast<const char*>(data), int(n), 0);
        if (sent < 0) { if (!wouldBlock()) open_ = false; return 0; }
        return size_t(sent);
    }
    int available() override {
        if (!open_) return 0;
        const int n = pending(socket_);
        if (n == 0) { // distinguish "nothing yet" from "closed by the broker"
            char probe;
            const int r = recv(socket_, &probe, 1, MSG_PEEK);
            if (r == 0 || (r < 0 && !wouldBlock())) open_ = false;
        }
        return n < 0 ? 0 : n;
    }
    int read() override {
        uint8_t c;
        return open_ && recv(socket_, reinterpret_cast<char*>(&c), 1, 0) == 1 ? c : -1;
    }
    void stop() override { if (socket_ != NoSocket) closeSocket(socket_); socket_ = NoSocket; open_ = false; }
    uint8_t connected() override { if (open_) available(); return open_; }
private:
    Socket socket_ = NoSocket;
    bool open_ = false;
};

static char expectedSmall[64];
static uint8_t* expectedBig = nullptr;
static const unsigned BigSize = 20000;
static bool gotSmall = false, gotBig = false, gotQos1 = false;
static void onMessage(char* topic, uint8_t* payload, unsigned int length) {
    const char* last = strrchr(topic, '/');
    if (!last) return;
    if (!strcmp(last, "/small")) gotSmall = length == strlen(expectedSmall) && !memcmp(payload, expectedSmall, length);
    if (!strcmp(last, "/big")) gotBig = length == BigSize && !memcmp(payload, expectedBig, length);
    if (!strcmp(last, "/qos1")) gotQos1 = length == 2 && !memcmp(payload, "q1", 2);
}

static bool pump(WilcoxonMqttClient& mqtt, uint32_t ms, bool* until) {
    const uint32_t start = millis();
    while (millis() - start < ms) {
        if (!mqtt.loop()) return false;
        if (until && *until) return true;
        delay(5);
    }
    return until == nullptr;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: live_broker_test <host> [port] [user] [password]\n"); return 2; }
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    const uint16_t port = argc > 2 ? uint16_t(atoi(argv[2])) : 1883;
    char id[48], base[96], topic[128];
    snprintf(id, sizeof(id), "wilcoxonmqtt-selftest-%lu", (unsigned long)millis());
    snprintf(base, sizeof(base), "wilcoxonmqtt/selftest/%s", id);
    snprintf(expectedSmall, sizeof(expectedSmall), "hello from %s", id);
    expectedBig = static_cast<uint8_t*>(malloc(BigSize));
    for (unsigned i = 0; i < BigSize; i++) expectedBig[i] = uint8_t('A' + i % 26);

    SocketClient net;
    WilcoxonMqttClient mqtt(net);
    mqtt.setServer(argv[1], port).setCallback(onMessage).setKeepAlive(3).setSocketTimeout(5);
    int failures = 0;
#define STEP(name, ok) do { const bool pass_ = (ok); printf("%s %s (state %d)\n", pass_ ? "PASS" : "FAIL", name, mqtt.state()); if (!pass_) failures++; } while (0)

    STEP("buffer 24 KB", mqtt.setBufferSize(24576));
    STEP("connect", argc > 3 ? mqtt.connect(id, argv[3], argc > 4 ? argv[4] : nullptr) : mqtt.connect(id));
    if (!mqtt.connected()) { printf("Cannot continue without a connection.\n"); return 1; }
    snprintf(topic, sizeof(topic), "%s/#", base);
    STEP("subscribe QoS 1", mqtt.subscribe(topic, 1));
    pump(mqtt, 300, nullptr); // let SUBACK arrive before publishing

    snprintf(topic, sizeof(topic), "%s/small", base);
    STEP("publish small", mqtt.publish(topic, expectedSmall));
    STEP("receive small", pump(mqtt, 3000, &gotSmall));

    snprintf(topic, sizeof(topic), "%s/big", base);
    STEP("publish 20000-byte payload", mqtt.publish(topic, expectedBig, BigSize));
    STEP("receive 20000-byte payload intact", pump(mqtt, 5000, &gotBig));

    // The broker downgrades to the subscription QoS (1), so this tests PUBACK handling only if it
    // forwards at QoS 1; a QoS 0 publish still arrives at QoS 0 and is accepted either way.
    snprintf(topic, sizeof(topic), "%s/qos1", base);
    STEP("publish for QoS 1 subscription", mqtt.publish(topic, "q1"));
    STEP("receive QoS 1 subscription message", pump(mqtt, 3000, &gotQos1));

    STEP("stay connected through keep-alive pings (8 s, keep-alive 3 s)", pump(mqtt, 8000, nullptr) && mqtt.connected());
    snprintf(topic, sizeof(topic), "%s/#", base);
    STEP("unsubscribe", mqtt.unsubscribe(topic));
    mqtt.disconnect();
    STEP("disconnect", mqtt.state() == WilcoxonMqttClient::Disconnected && !net.connected());

    WilcoxonMqttClient refused(net);
    refused.setServer(argv[1], port).setSocketTimeout(5);
    const bool rejected = !refused.connect(id, "wilcoxonmqtt-wrong-user", "wrong-password");
    printf("INFO wrong credentials: %s, state %d (brokers that allow anonymous logins accept them)\n",
           rejected ? "refused" : "accepted", refused.state());
    refused.disconnect();
    free(expectedBig);
    printf(failures ? "%d step(s) failed\n" : "PASS: live broker test (real broker, PC network, not the IOC)\n", failures);
    return failures ? 1 : 0;
}
