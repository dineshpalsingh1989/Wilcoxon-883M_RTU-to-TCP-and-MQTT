#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WilcoxonMqtt.h>
#include <time.h>
#include "W5500SecureMqttVersion.h"

// Call exclusively from Arduino setup()/loop(), on core 0. Connection attempts
// block during TCP/TLS/MQTT handshakes; this is not an asynchronous TLS engine.
class W5500SecureMqtt {
public:
    enum class Status { Unconfigured, WaitingNetwork, WaitingTime, RetryWait,
                        Connecting, Connected, ConnectFailed };
    W5500SecureMqtt() : mqtt_(tls_) {}
    W5500SecureMqtt(const W5500SecureMqtt&) = delete;
    W5500SecureMqtt& operator=(const W5500SecureMqtt&) = delete;

    // All strings and the trust object must outlive this object. Use a DNS
    // hostname matching the certificate. Credentials are never logged.
    bool begin(const char* host, const char* clientId,
               const BearSSL::X509List& trust, const char* user = nullptr,
               const char* password = nullptr, uint16_t port = 8883,
               uint16_t packetBufferSize = 1024) {
        stop();
        if (!host || !*host || !clientId || !*clientId || !port || !trust.getCount()) return false;
        verifyCertificate_ = true;
        tls_.setTrustAnchors(&trust);
        return configure(host, clientId, user, password, port, packetBufferSize);
    }
    // Explicit opt-in: TLS encryption without authenticating the broker.
    // No CA or certificate-validity clock is required in this mode.
    bool beginInsecure(const char* host, const char* clientId,
                       const char* user = nullptr, const char* password = nullptr,
                       uint16_t port = 8883, uint16_t packetBufferSize = 1024) {
        stop();
        if (!host || !*host || !clientId || !*clientId || !port) return false;
        verifyCertificate_ = false;
        tls_.setInsecure();
        return configure(host, clientId, user, password, port, packetBufferSize);
    }
    bool tick(bool networkReady, time_t now = time(nullptr)) {
        if (!configured_) return false;
        if (!networkReady) {
            tls_.stop();
            status_ = Status::WaitingNetwork;
            return false;
        }
        // Sanity gate only: the application must obtain trustworthy current time.
        if (verifyCertificate_ && now < 1704067200) {
            tls_.stop();
            status_ = Status::WaitingTime;
            return false;
        }
        if (mqtt_.connected() && mqtt_.loop()) {
            status_ = Status::Connected;
            return true;
        }
        const uint32_t ms = millis();
        if (attempted_ && uint32_t(ms - lastAttempt_) < retryDelay_) {
            status_ = Status::RetryWait;
            return false;
        }
        attempted_ = true;
        status_ = Status::Connecting;
        if (verifyCertificate_) tls_.setX509Time(now);
        const bool ok = mqtt_.connect(id_, user_, password_);
        lastAttempt_ = millis();
        // Each attempt blocks the caller; back off 10 s -> 60 s while the broker keeps failing.
        retryDelay_ = ok ? 10000 : (retryDelay_ >= 30000 ? 60000 : retryDelay_ * 2);
        status_ = ok ? Status::Connected : Status::ConnectFailed;
        return ok;
    }
    // Lets the next tick() connect at once instead of waiting out the retry delay (for manual tests).
    void retryNow() { attempted_ = false; }
    void stop() {
        mqtt_.disconnect();
        tls_.stop();
        configured_ = attempted_ = false;
        retryDelay_ = 10000;
        status_ = Status::Unconfigured;
    }
    bool publish(const char* topic, const char* payload, bool retained = false) {
        return configured_ && mqtt_.connected() && mqtt_.publish(topic, payload, retained);
    }
    bool subscribe(const char* topic, uint8_t qos = 0) {
        return configured_ && mqtt_.connected() && mqtt_.subscribe(topic, qos);
    }
    void setCallback(WilcoxonMqttClient::Callback callback) { mqtt_.setCallback(callback); }
    // For existing publishers/subscribers. Connection lifecycle stays in tick().
    WilcoxonMqttClient& protocolClient() { return mqtt_; }
    Status status() const { return status_; }
    int mqttError() { return mqtt_.state(); }
    int tlsError(char* buffer = nullptr, size_t length = 0) {
        return tls_.getLastSSLError(buffer, length);
    }
private:
    bool configure(const char* host, const char* clientId, const char* user,
                   const char* password, uint16_t port, uint16_t packetBufferSize) {
        host_ = host; id_ = clientId; user_ = user; password_ = password;
        tls_.setSSLVersion(BR_TLS12, BR_TLS12);
        mqtt_.setServer(host_, port);
        mqtt_.setSocketTimeout(5); // CONNACK wait only; the TLS handshake has its own connect timeout
        mqtt_.setKeepAlive(30);
        if (!mqtt_.setBufferSize(packetBufferSize)) return false;
        configured_ = true;
        status_ = Status::WaitingNetwork;
        return true;
    }
    BearSSL::WiFiClientSecure tls_;
    WilcoxonMqttClient mqtt_;
    const char* host_ = nullptr;
    const char* id_ = nullptr;
    const char* user_ = nullptr;
    const char* password_ = nullptr;
    uint32_t lastAttempt_ = 0;
    uint32_t retryDelay_ = 10000;
    bool configured_ = false, attempted_ = false;
    bool verifyCertificate_ = true;
    Status status_ = Status::Unconfigured;
};
