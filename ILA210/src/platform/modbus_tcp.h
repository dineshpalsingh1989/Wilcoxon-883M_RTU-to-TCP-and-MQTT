#pragma once
#include "platform/platform.h"
#include <WiFi.h>
#if GATEWAY_ETHERNET
#include <ModbusAPI.h>
#include <ModbusTCPTemplate.h>
// Arduino-Pico exposes accept() directly; ESP32 needs the library's wrapper.
using ModbusTCP = ModbusAPI<ModbusTCPTemplate<WiFiServer, WiFiClient>>;
#else
#include <ModbusTCP.h>
#endif
