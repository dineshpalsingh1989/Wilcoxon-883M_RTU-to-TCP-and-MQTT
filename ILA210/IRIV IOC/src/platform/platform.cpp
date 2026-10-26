#include "platform/platform.h"
#include "config/board_pins.h"

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
bool onFallback = false, usingStatic = false;
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
// Task stacks live in static RAM, below the heap. BearSSL (MQTT over TLS) runs on its own stack
// taken from the heap, and the RP2350's FreeRTOS port faults as soon as a task's stack pointer
// goes below the task stack (PSPLIM). With the task stack at a lower address than the BearSSL
// stack, that switch is allowed; with heap-allocated task stacks the TLS handshake froze the chip.
// Both tasks stay on core 0, the only core Arduino-Pico allows for networking.
namespace {
constexpr size_t TaskSlots = 2, MaxTaskStackBytes = 16384;
StackType_t taskStacks[TaskSlots][MaxTaskStackBytes / sizeof(StackType_t)];
StaticTask_t taskBuffers[TaskSlots];
size_t tasksUsed = 0;
}
bool startTask(TaskFunction_t task, const char* name, uint32_t stackBytes, UBaseType_t priority, int) {
    if (tasksUsed >= TaskSlots || stackBytes > MaxTaskStackBytes) return false;
    const size_t slot = tasksUsed++;
    TaskHandle_t handle = xTaskCreateStatic(task, name, stackBytes / sizeof(StackType_t), nullptr, priority,
                                            taskStacks[slot], &taskBuffers[slot]);
    if (!handle) return false;
    vTaskCoreAffinitySet(handle, 1 << 0);
    return true;
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
String netDescription() {
    if (onFallback) return "Ethernet (fixed address, no DHCP)";
    return usingStatic ? "Ethernet (static IP)" : "Ethernet (DHCP)";
}
String netSignal() { return "Wired"; }
int netRssi() { return 0; }

namespace {
uint32_t hostOrder(const IPAddress& a) {
    return (uint32_t(a[0]) << 24) | (uint32_t(a[1]) << 16) | (uint32_t(a[2]) << 8) | a[3];
}
bool usableUnicast(uint32_t address) {
    const uint8_t first = address >> 24;
    return first != 0 && first != 127 && first < 224;
}
}
bool parseIPv4(const String& text, IPAddress& out, bool allowEmpty) {
    out = IPAddress(0, 0, 0, 0);
    if (!text.length()) return allowEmpty;
    uint8_t parts[4];
    int count = 0, value = -1;
    for (size_t i = 0; i <= text.length(); i++) {
        const char c = i < text.length() ? text[i] : '.';
        if (c >= '0' && c <= '9') {
            value = (value < 0 ? 0 : value * 10) + (c - '0');
            if (value > 255) return false;
        } else if (c == '.' && value >= 0 && count < 4) {
            parts[count++] = value;
            value = -1;
        } else {
            return false;
        }
    }
    if (count != 4) return false;
    out = IPAddress(parts[0], parts[1], parts[2], parts[3]);
    return true;
}
bool ethernetCheckStatic(const EthernetAddress& address, String& error) {
    const uint32_t ip = hostOrder(address.ip), mask = hostOrder(address.mask);
    const uint32_t gateway = hostOrder(address.gateway), dns = hostOrder(address.dns);
    const uint32_t hostBits = ~mask;
    if (mask < 0xFF000000 || mask > 0xFFFFFFFC || (hostBits & (hostBits + 1))) {
        error = "Subnet mask must be contiguous, from 255.0.0.0 to 255.255.255.252"; return false;
    }
    if (!usableUnicast(ip) || !(ip & hostBits) || (ip & hostBits) == hostBits) {
        error = "IP address is not a usable host address on this subnet"; return false;
    }
    if (gateway && (((gateway ^ ip) & mask) || gateway == ip || !(gateway & hostBits) || (gateway & hostBits) == hostBits)) {
        error = "Gateway must be another host address on the same subnet as the IP address"; return false;
    }
    if (dns && !usableUnicast(dns)) { error = "DNS server is not a usable address"; return false; }
    return true;
}
bool ethernetUsesStatic() { return usingStatic; }
IPAddress ethernetMask() { return eth.subnetMask(); }
IPAddress ethernetGateway() { return eth.gatewayIP(); }
IPAddress ethernetDns() { return eth.dnsIP(); }

bool ethernetBegin(const EthernetAddress& address) {
    pinMode(PIN_W5500_RST, OUTPUT);
    digitalWrite(PIN_W5500_RST, LOW); delay(10);
    digitalWrite(PIN_W5500_RST, HIGH); delay(100);
    SPI.setRX(PIN_W5500_MISO); SPI.setTX(PIN_W5500_MOSI); SPI.setSCK(PIN_W5500_SCK);
    eth.setHostname(ETHERNET_HOSTNAME);
    String error;
    if (address.useStatic && ethernetCheckStatic(address, error)) {
        // ESP argument order (ip, gateway, mask, dns); the mask starts with 255, so lwIP keeps it.
        const IPAddress dns = hostOrder(address.dns) ? address.dns : address.gateway;
        usingStatic = eth.config(address.ip, address.gateway, address.mask, dns);
    }
    if (!eth.begin()) return false;
    if (usingStatic) return true; // the address is set at once; the cable may be connected later
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
