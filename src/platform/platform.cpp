#include "platform/platform.h"
#include "config/config.h"

#if GATEWAY_ETHERNET
#include <LittleFS.h>
#include <W5500lwIP.h>
#include <WiFi.h>
#include <time.h>
#else
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#endif

namespace gw {

#if GATEWAY_ETHERNET
// ---------------------------------------------------------------- RP2350 (IRIV IO Controller)
namespace {
Wiznet5500lwIP eth(PIN_W5500_CS, SPI, PIN_W5500_INT);
bool onFallback = false;
SemaphoreHandle_t fsMutex = nullptr;
}

void platformBegin() {
    fsMutex = xSemaphoreCreateMutex();
    if (!LittleFS.begin()) { LittleFS.format(); LittleFS.begin(); }
}
bool fsLock() { return fsMutex && xSemaphoreTake(fsMutex, pdMS_TO_TICKS(2000)) == pdTRUE; }
void fsUnlock() { xSemaphoreGive(fsMutex); }
void restart() { rp2040.reboot(); }
uint32_t freeHeap() { return rp2040.getFreeHeap(); }
String resetReason() {
    switch (rp2040.getResetReason()) {
        case RP2040::PWRON_RESET: return "Power On";
        case RP2040::RUN_PIN_RESET: return "External Pin Reset";
        case RP2040::SOFT_RESET: return "Software Reset";
        case RP2040::WDT_RESET: return "Watchdog (Background Loop Stuck)";
        case RP2040::DEBUG_RESET: return "Debugger Reset";
        case RP2040::GLITCH_RESET: return "Power Glitch";
        case RP2040::BROWNOUT_RESET: return "Brownout";
        default: return "Unknown";
    }
}
// The RP2350 hardware watchdog allows at most ~16.7 s, too short for an MQTT/TLS connect.
// So it works like the ESP32's 60 s task watchdog: the background task only records a check-in,
// and a top-priority supervisor task feeds the hardware watchdog while check-ins are recent.
// A stuck background task stops the feeding after 60 s; a stuck system stops it after 8 s.
namespace {
volatile uint32_t lastCheckIn = 0;
void watchdogSupervisor(void*) {
    for (;;) {
        if (millis() - lastCheckIn < 60000) rp2040.wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}
void watchdogStart() {
    lastCheckIn = millis();
    rp2040.wdt_begin(8000);
    xTaskCreate(watchdogSupervisor, "Watchdog", 1024, nullptr, configMAX_PRIORITIES - 1, nullptr);
}
void watchdogFeed() { lastCheckIn = millis(); }
bool startTask(TaskFunction_t task, const char* name, uint32_t stackBytes, UBaseType_t priority, int) {
    return xTaskCreate(task, name, stackBytes / sizeof(StackType_t), nullptr, priority, nullptr) == pdPASS;
}
String deviceId() { return String(rp2040.getChipID()); }
void startTimeSync(long gmtOffsetSec, int daylightOffsetSec, const char* server) {
    // POSIX TZ offsets are west-positive, so UTC+8 is written "GWT-8:00".
    long offset = gmtOffsetSec + daylightOffsetSec;
    char tz[24];
    snprintf(tz, sizeof(tz), "GWT%c%ld:%02ld", offset >= 0 ? '-' : '+', labs(offset) / 3600, (labs(offset) % 3600) / 60);
    setenv("TZ", tz, 1);
    tzset();
    NTP.begin(server);
}

bool netConnected() { return eth.connected(); }
IPAddress netIP() { return eth.localIP(); }
String netDescription() { return onFallback ? String("Ethernet (fixed address, no DHCP)") : String("Ethernet (DHCP)"); }
String netSignal() { return "Wired"; }
int netRssi() { return 0; }

bool ethernetBegin() {
    pinMode(PIN_W5500_RST, OUTPUT);
    digitalWrite(PIN_W5500_RST, LOW); delay(10);
    digitalWrite(PIN_W5500_RST, HIGH); delay(100);
    SPI.setRX(PIN_W5500_MISO); SPI.setTX(PIN_W5500_MOSI); SPI.setSCK(PIN_W5500_SCK);
    eth.setHostname(ETHERNET_HOSTNAME);
    if (!eth.begin()) return false;
    const uint32_t started = millis();
    while (!eth.connected() && millis() - started < 10000) delay(100);
    if (eth.connected()) return true;
    ethernetUseFallback();
    return false;
}
void ethernetUseFallback() {
    IPAddress ip, mask(255, 255, 255, 0);
    ip.fromString(ETHERNET_FALLBACK_IP);
    eth.end();
    eth.config(ip, IPAddress(0, 0, 0, 0), mask);
    eth.begin();
    onFallback = true;
}
bool ethernetOnFallback() { return onFallback; }
String ethernetMac() {
    uint8_t mac[6]; eth.macAddress(mac);
    char text[18];
    snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return text;
}

#else
// ---------------------------------------------------------------- ESP32 boards
void platformBegin() {}
void restart() { ESP.restart(); }
uint32_t freeHeap() { return ESP.getFreeHeap(); }
String resetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON: return "Power On";
        case ESP_RST_EXT: return "External Pin Reset";
        case ESP_RST_SW: return "Software Reset";
        case ESP_RST_PANIC: return "Crash (Panic/Exception)";
        case ESP_RST_INT_WDT: return "Internal Watchdog";
        case ESP_RST_TASK_WDT: return "Task WDT (Background Loop Stuck)";
        case ESP_RST_WDT: return "Other Watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep Sleep Wake";
        case ESP_RST_BROWNOUT: return "Brownout";
        default: return "Unknown (" + String(reason) + ")";
    }
}
void watchdogStart() {
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 60000,
        .idle_core_mask = (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_init(&twdt_config);
#else
    esp_task_wdt_init(60, true);
#endif
    esp_task_wdt_add(NULL);
}
void watchdogFeed() { esp_task_wdt_reset(); }
bool startTask(TaskFunction_t task, const char* name, uint32_t stackBytes, UBaseType_t priority, int core) {
    if (core < 0) return xTaskCreate(task, name, stackBytes, nullptr, priority, nullptr) == pdPASS;
    return xTaskCreatePinnedToCore(task, name, stackBytes, nullptr, priority, nullptr, core) == pdPASS;
}
String deviceId() { String mac = WiFi.macAddress(); mac.replace(":", ""); return mac; }
void startTimeSync(long gmtOffsetSec, int daylightOffsetSec, const char* server) {
    configTime(gmtOffsetSec, daylightOffsetSec, server);
}

bool netConnected() { return WiFi.status() == WL_CONNECTED; }
IPAddress netIP() { return netConnected() ? WiFi.localIP() : WiFi.softAPIP(); }
String netDescription() { return netConnected() ? "Connected to " + WiFi.SSID() : String("AP Mode"); }
String netSignal() { return netConnected() ? String(WiFi.RSSI()) + " dBm" : String("N/A"); }
int netRssi() { return WiFi.RSSI(); }
#endif

}
