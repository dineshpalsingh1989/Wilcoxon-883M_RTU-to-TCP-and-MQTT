#pragma once
#include "core/globals.h"

// Recording functions run under dataMutex, shared with the web snapshot handler.
void setupMonitoring();
void recordMetricsHealth(bool success, uint8_t error);
void recordCaptureResult(Axis axis, const char* result, const char* transmission);
void handleMonitoring();
void handleAlarmSettings();
// Alarm and link state for the IRIV relays. Caller holds dataMutex.
bool monitoringAlarmActive();
bool monitoringCommunicationFault();
bool monitoringFreshReading(); // a good reading recently, as the dashboard's "Connected" state
extern bool firmwareUpdateActive; // read/write only under dataMutex
extern bool backgroundPausedForUpdate;
