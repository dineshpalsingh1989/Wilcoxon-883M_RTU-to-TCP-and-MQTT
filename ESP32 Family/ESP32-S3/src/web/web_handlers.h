#pragma once
void handleGithubUpdates();
void handleFirmwareInfo();
void handleMonitoringScript();
#include "core/globals.h"

// Sends one dynamic piece of a chunked page. An empty chunk would end the response early, so it is skipped.
inline void sendPageText(const String& text) { if (text.length()) server.sendContent(text); }

void registerWebRoutes();
#if GATEWAY_ETHERNET
void handleNetworkSettings();
#endif

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
