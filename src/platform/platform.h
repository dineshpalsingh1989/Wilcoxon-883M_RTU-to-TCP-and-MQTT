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
#include <Preferences.h>
#endif

namespace gw {
// Call first in setup(): mounts the settings store where needed.
void platformBegin();
void restart();
uint32_t freeHeap();
String resetReason();
// Background task watchdog: start once from that task, then feed it in long loops.
void watchdogStart();
void watchdogFeed();
// For one bounded blocking call (MQTT/TLS connect) that can outlast the RP2350's 16 s hardware
// watchdog: until watchdogBlockingEnd() or the limit passes, watchdogService() keeps it fed.
// If the call hangs past the limit, feeding stops and the watchdog resets the chip as before.
void watchdogBlockingBegin(uint32_t limitMs);
void watchdogBlockingEnd();
// Call regularly from a task other than the one that blocks (the web server task).
void watchdogService();
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
// Starts Ethernet with DHCP; without a DHCP answer it falls back to FALLBACK_IP.
bool ethernetBegin();
// Switches to FALLBACK_IP, for a laptop plugged in directly (user button held 5 s).
void ethernetUseFallback();
bool ethernetOnFallback();
String ethernetMac();
#endif
}
