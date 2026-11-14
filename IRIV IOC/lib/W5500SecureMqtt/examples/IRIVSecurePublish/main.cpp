#include <Arduino.h>
#include <SPI.h>
#include <IRIVIOCHardware.h>
#include <IRIVIOCSettings.h>
#include <W5500SecureMqtt.h>
#include <LittleFS.h>
#include <WiFi.h>

// Uses the IRIV IOC board variant's existing Ethernet pin definitions.
irivioc::Ethernet ethernet(PIN_W5500_CS, SPI, PIN_W5500_INT);
W5500SecureMqtt mqtt;
// Replace with your broker's public CA PEM; never commit credentials/private keys.
static const char caPem[] = "";
BearSSL::X509List trust(caPem);
// This example uses settings only in setup(), with no other filesystem users.
bool lockStore() { return true; }
void unlockStore() {}
irivioc::Settings settings(lockStore, unlockStore);
bool configured = false;

void setup() {
    Serial.begin(115200);
    // Mount only: never format a user's filesystem on a mount failure.
    if (LittleFS.begin() && settings.begin("example")) {
        Serial.println(settings.getString("label", "IRIV IOC example"));
        settings.end();
    }
    pinMode(PIN_W5500_RST, OUTPUT);
    digitalWrite(PIN_W5500_RST, LOW);
    delay(10);
    digitalWrite(PIN_W5500_RST, HIGH);
    delay(100);
    SPI.setRX(PIN_W5500_MISO);
    SPI.setTX(PIN_W5500_MOSI);
    SPI.setSCK(PIN_W5500_SCK);
    if (!ethernet.begin("iriv-secure-mqtt")) {
        Serial.println("Ethernet initialization failed");
        return;
    }
    NTP.begin("pool.ntp.org");
    if (!*caPem) {
        Serial.println("Configure broker hostname and public CA PEM before connecting");
        return;
    }
    // Use a unique client ID per device. Add credentials locally if required.
    configured = mqtt.begin("broker.example.com", "iriv-bench-unique-id", trust);
}

void loop() {
    static uint32_t lastPublish = 0;
    if (configured && mqtt.tick(ethernet.connected())) {
        if (uint32_t(millis() - lastPublish) >= 10000) {
            lastPublish = millis();
            Serial.println(mqtt.publish("bench/iriv/status", "test message")
                           ? "Published" : "Publish failed");
        }
    } else if (mqtt.status() == W5500SecureMqtt::Status::ConnectFailed) {
        char error[160];
        mqtt.tlsError(error, sizeof(error));
        Serial.println(error);
        Serial.println(mqtt.mqttError());
    }
    delay(10);
}
