#pragma once
// Chip and network differences behind one interface.
// ESP32 boards use Wi-Fi; the IRIV IO Controller (RP2350) uses wired Ethernet (W5500).
#include <Arduino.h>
#include <IPAddress.h>

#if defined(ARDUINO_ARCH_RP2040)
#define GATEWAY_ETHERNET 1
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "platform/fs_preferences.h"
#else
#define GATEWAY_ETHERNET 0
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "platform/fs_preferences.h"
#endif

namespace gw {
// Call first in setup(): mounts the settings store where needed.
void platformBegin();
void restart();
uint32_t freeHeap();
String resetReason();
// Background task watchdog: start once from that task, then feed it in long loops.
// The chip restarts if the background task does not feed it for 60 s, on every board.
void watchdogStart();
void watchdogFeed();
// stackBytes is in bytes on every chip; core is a hint where tasks can be pinned.
bool startTask(TaskFunction_t task, const char* name, uint32_t stackBytes, UBaseType_t priority, int core);
// Unique hardware ID without separators (Wi-Fi MAC or RP2350 board ID).
String deviceId();
void startTimeSync(long gmtOffsetSec, int daylightOffsetSec, const char* server);

// Network status for pages, MQTT health and the alarm logic.
bool netConnected();
IPAddress netIP();
String netDescription(); // e.g. "Connected to Office" or "Ethernet (DHCP)"
String netSignal();      // e.g. "-55 dBm" or "Wired"
int netRssi();           // 0 on wired Ethernet

#if GATEWAY_ETHERNET
// Serializes LittleFS use between tasks (settings and firmware upload).
bool fsLock();
void fsUnlock();
// Saved Ethernet address. useStatic false means DHCP; dns 0.0.0.0 means "use the gateway".
struct EthernetAddress {
    bool useStatic = false;
    IPAddress ip, mask, gateway, dns;
};
// Strict dotted IPv4 ("192.168.1.20"). Empty text gives 0.0.0.0, accepted only when allowEmpty.
bool parseIPv4(const String& text, IPAddress& out, bool allowEmpty);
// Checks a static address set (one subnet, usable host addresses); error names the first problem.
bool ethernetCheckStatic(const EthernetAddress& address, String& error);
// Starts Ethernet with the static address, or with DHCP falling back to FALLBACK_IP without an answer.
bool ethernetBegin(const EthernetAddress& address);
bool ethernetUsesStatic();
IPAddress ethernetMask();
IPAddress ethernetGateway();
IPAddress ethernetDns();
// Switches to FALLBACK_IP, for a laptop plugged in directly (user button held 5 s).
void ethernetUseFallback();
bool ethernetOnFallback();
String ethernetMac();
#endif
}
