#pragma once
#include "core/globals.h"

void registerWebRoutes();

void handleRoot(); void handleMetrics(); void handleFullWaveform(); void handleFullSpectrum();
void handleSettings(); void handleWifiSettings(); void handleSaveWifi();
void handleMqttSettings(); void handleSaveMqtt();
void handleRs485Settings(); void handleSaveRs485();
void handleTriggerCapture(); void handleSetIntervals();
void handleGetIntervals(); void handleScanWifi(); void handleNotFound();
void handleGetMetricsLogData(); void handleMetricsLogControl(); void handleDownloadMetricsCsv();
void handleClearMetricsLog(); void handleClearDynamicData(); void handleReboot();
void handleDeviceInfo(); void handleModbusTcpSettings(); void handleSaveModbusTcp();
void handleSetMetricsInterval();
void handleTimeSettings(); void handleSaveTimeSettings(); void handleSetManualTime();
void handleClearSystemLog();
void handleMqttStatus();
