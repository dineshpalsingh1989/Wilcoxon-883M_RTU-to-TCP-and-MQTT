#pragma once
#include "core/globals.h"

void webServerStep();
void backgroundStep();
// Resumes sensor polling after it stopped for repeated failures; the next read runs at once.
void requestSensorReconnect();
#if GATEWAY_ETHERNET
// Silences the buzzer, resets the latched alarm relay and retries a paused sensor.
void acknowledgeAlarms(const char* source);
#endif
void webServerLoop(void * pvParameters);
void backgroundLoop(void * pvParameters);
