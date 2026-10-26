#pragma once
#include <WebServer.h>
#include "core/sensor_policy.h"
#include <WilcoxonMqtt.h>

namespace ioc {
// Sampling owns only DI0..DI10, AN0/AN1 and DO2/DO3. DO0/DO1 remain sensor outputs.
bool begin(bool (*requestCapture)());
void registerRoutes(WebServer& server);
void setSensorAlarms(bool alert, bool alarm, const gateway::SensorHealth& health);
// Called only by the existing network/background task.
void serviceMqtt(WilcoxonMqttClient& client, const String& baseTopic);
}
