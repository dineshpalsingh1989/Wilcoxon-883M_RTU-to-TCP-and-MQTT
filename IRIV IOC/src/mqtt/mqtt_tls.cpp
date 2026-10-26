#include "core/modules.h"
#if GATEWAY_ETHERNET
#include <LittleFS.h>
#include <W5500SecureMqtt.h>
#include <memory>

namespace {
constexpr size_t MaxCertificateBytes = 8192;
constexpr const char* CertificatePath = "/mqtt-ca.pem";
W5500SecureMqtt secureMqtt;
std::unique_ptr<BearSSL::X509List> trust;
String clientId;
bool ready = false;
bool subscribed = false;
// Connection history for the log and the Libraries page.
uint32_t connectedAt = 0, connects = 0, drops = 0, lastSessionSeconds = 0;
bool sessionUp = false;
int lastDropState = 0;
}

bool mqttCertificateInstalled() {
    if (!gw::fsLock()) return false;
    File file = LittleFS.open(CertificatePath, "r");
    const bool ok = file && file.size() > 0 && file.size() <= MaxCertificateBytes;
    if (file) file.close();
    gw::fsUnlock();
    return ok;
}

void setupSecureMqtt() {
    clientId = "iriv-883m-" + gw::deviceId();
    if (mqtt_skip_cert_validation) {
        ready = secureMqtt.beginInsecure(mqtt_server_str.c_str(), clientId.c_str(),
            mqtt_user_str.c_str(), mqtt_pass_str.c_str(), mqtt_port_val, 4096);
        logMessage("[MQTT] TLS certificate verification disabled by settings");
    } else {
        String pem;
        if (gw::fsLock()) {
            File file = LittleFS.open(CertificatePath, "r");
            if (file && file.size() <= MaxCertificateBytes) pem = file.readString();
            if (file) file.close();
            gw::fsUnlock();
        }
        if (!pem.length()) {
            snprintf(mqttStatus, sizeof(mqttStatus), "TLS: install broker CA in MQTT settings");
            return;
        }
        trust.reset(new BearSSL::X509List(pem.c_str()));
        ready = secureMqtt.begin(mqtt_server_str.c_str(), clientId.c_str(), *trust,
            mqtt_user_str.c_str(), mqtt_pass_str.c_str(), mqtt_port_val, 4096);
    }
    if (!ready) {
        snprintf(mqttStatus, sizeof(mqttStatus), "TLS: invalid CA/configuration or insufficient memory");
        return;
    }
    secureMqtt.setCallback(mqttCallback);
    mqttClient = &secureMqtt.protocolClient();
    snprintf(mqttStatus, sizeof(mqttStatus), "%s", mqtt_skip_cert_validation
        ? "TLS (certificate unchecked): waiting for network" : "TLS: waiting for network and time");
}

void serviceSecureMqtt() {
    if (!ready || !mqttEnabled) return;
    const bool wasConnected = mqttClient->connected();
    if (sessionUp && !wasConnected) {
        sessionUp = false;
        drops++;
        lastSessionSeconds = (millis() - connectedAt) / 1000;
        lastDropState = secureMqtt.mqttError();
        char error[80] = {};
        const int tlsCode = secureMqtt.tlsError(error, sizeof(error));
        logMessage("[MQTT] TLS session closed after " + String(lastSessionSeconds) + " s, MQTT state " +
                   String(lastDropState) + ", TLS " + String(tlsCode) + (tlsCode ? String(": ") + error : String()));
        snprintf(mqttStatus, sizeof(mqttStatus), "TLS session closed after %lu s (MQTT state %d); reconnecting",
                 (unsigned long)lastSessionSeconds, lastDropState);
    }
    // TLS handshakes are synchronous: defer new attempts during captures.
    if (!wasConnected && (currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum || isPublishingCapture)) return;
    const bool connected = secureMqtt.tick(gw::netConnected());
    if (!connected) subscribed = false;
    if (connected) {
        if (!wasConnected) {
            subscribed = false;
            connects++; connectedAt = millis(); sessionUp = true;
            logMessage(mqtt_skip_cert_validation ? "[MQTT] TLS connected (certificate unchecked)"
                                                 : "[MQTT] Certificate-verified TLS connected");
            lastHealthPublishTime = millis() - healthPublishInterval;
        }
        if (!subscribed) subscribed = mqttClient->subscribe(mqtt_command_topic.c_str());
        snprintf(mqttStatus, sizeof(mqttStatus), "%s%s", mqtt_skip_cert_validation
            ? "Connected (TLS, certificate unchecked)" : "Connected (verified TLS)",
            subscribed ? "" : "; subscription pending");
        if (millis() - lastHealthPublishTime >= healthPublishInterval) {
            lastHealthPublishTime = millis();
            publishDeviceHealth();
        }
    } else {
        switch (secureMqtt.status()) {
        case W5500SecureMqtt::Status::WaitingNetwork:
            snprintf(mqttStatus, sizeof(mqttStatus), "TLS: waiting for Ethernet"); break;
        case W5500SecureMqtt::Status::WaitingTime:
            snprintf(mqttStatus, sizeof(mqttStatus), "TLS: waiting for correct time (NTP or Time settings)"); break;
        case W5500SecureMqtt::Status::ConnectFailed: {
            char error[80] = {};
            const int tlsCode = secureMqtt.tlsError(error, sizeof(error));
            snprintf(mqttStatus, sizeof(mqttStatus), "TLS %d: %.70s; MQTT %d", tlsCode, error, secureMqtt.mqttError());
            logMessage(String("[MQTT] ") + mqttStatus);
            break;
        }
        default: break; // Keep the useful failure message throughout retry delay.
        }
    }
}

bool connectSecureMqttNow() {
    if (!ready) return false;
    secureMqtt.retryNow();
    return secureMqtt.tick(gw::netConnected());
}

void addSecureMqttLibraryStatus(JsonObject out) {
    out["configured"] = ready;
    const char* text = "Not configured";
    switch (secureMqtt.status()) {
    case W5500SecureMqtt::Status::Unconfigured: text = "Not configured"; break;
    case W5500SecureMqtt::Status::WaitingNetwork: text = "Waiting for Ethernet"; break;
    case W5500SecureMqtt::Status::WaitingTime: text = "Waiting for correct time"; break;
    case W5500SecureMqtt::Status::RetryWait: text = "Waiting to retry"; break;
    case W5500SecureMqtt::Status::Connecting: text = "Connecting"; break;
    case W5500SecureMqtt::Status::Connected: text = "Connected"; break;
    case W5500SecureMqtt::Status::ConnectFailed: text = "Connect failed"; break;
    }
    out["state"] = text;
    out["connects"] = connects;
    out["drops"] = drops;
    out["last_session_s"] = lastSessionSeconds;
    out["last_drop_state"] = lastDropState;
    char error[80] = {};
    out["tls_error"] = secureMqtt.tlsError(error, sizeof(error));
    out["tls_error_text"] = error;
    out["mqtt_state"] = secureMqtt.mqttError();
}

void handleMqttCertificate() {
    if (firmwareUpdateActive || currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum || isPublishingCapture) {
        server.send(409, "text/plain", "Device busy; retry after capture/update"); return;
    }
    const String pem = server.arg("pem");
    if (!pem.length() || pem.length() > MaxCertificateBytes || pem.indexOf("PRIVATE KEY") >= 0) {
        server.send(400, "text/plain", "Supply public CA certificates only, up to 8192 bytes"); return;
    }
    BearSSL::X509List parsed(pem.c_str());
    if (!parsed.getCount() || parsed.getCount() > 8) {
        server.send(400, "text/plain", "Invalid PEM certificate bundle (maximum 8 certificates)"); return;
    }
    if (!gw::fsLock()) { server.send(503, "text/plain", "Settings storage busy"); return; }
    File file = LittleFS.open("/mqtt-ca.tmp", "w");
    bool ok = file && file.print(pem) == pem.length();
    if (file) { file.flush(); ok = ok && !file.getWriteError(); file.close(); }
    ok = ok && LittleFS.rename("/mqtt-ca.tmp", CertificatePath);
    gw::fsUnlock();
    server.send(ok ? 200 : 500, "text/plain", ok ? "CA saved; save MQTT settings to restart" : "Certificate write failed");
}
#endif
