#pragma once
#include "core/globals.h"

// Recording functions run under dataMutex, shared with the web snapshot handler.
void setupMonitoring();
void recordMetricsHealth(bool success, uint8_t error);
void recordCaptureResult(Axis axis, const char* result, const char* transmission);
void handleMonitoring();
void handleAlarmSettings();
extern bool firmwareUpdateActive; // read/write only under dataMutex
extern bool backgroundPausedForUpdate;
