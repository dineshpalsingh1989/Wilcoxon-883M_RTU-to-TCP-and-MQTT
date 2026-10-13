#pragma once
// Shared libraries and extern declarations for all global state.
// Definitions live in core/globals.cpp.

#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "time.h"
#include <esp_task_wdt.h>

// OTA
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Update.h>

// Modbus
#include <ModbusMaster.h> // RS485 Master
#include <ModbusTCP.h>    // TCP Server
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <map>

// FreeRTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "config/config.h"
#include "core/types.h"

extern SemaphoreHandle_t dataMutex;
extern bool shouldReboot;
extern Adafruit_NeoPixel pixels;

// Device info
extern String sensor_model, sensor_serial, sensor_vendor_url, sensor_part_number, sensor_fw_version;
extern const char* FIRMWARE_VERSION;

// NTP & time
extern const char* ntpServer;
extern long gmtOffset_sec;
extern int daylightOffset_sec;

// MQTT
extern bool mqttEnabled, mqtt_use_tls, mqtt_skip_cert_validation, mqtt_publish_progress;
extern String mqtt_server_str;
extern int mqtt_port_val;
extern String mqtt_user_str, mqtt_pass_str, mqtt_topic_str, mqtt_command_topic, mqtt_status_topic;
extern char mqttStatus[128];

// Wi-Fi & web server
extern WebServer server;
extern Preferences preferences;
extern WiFiClient espClient;
extern WiFiClientSecure espClientSecure;
extern PubSubClient* mqttClient;

// Modbus RTU / TCP
extern uint8_t modbus_slave_id_val;
extern long modbus_baud_rate_val;
extern ModbusMaster node;
extern bool modbusTcpEnabled;
extern uint16_t modbusTcpPort;
extern ModbusTCP modbusTCPServer;

// Global state
extern char systemStatus[128];
extern char trafficLightStatus[20];
extern String globalLogBuffer, lastResetReason;
extern DynamicDataState currentDataState;
extern CaptureMode requestedCapture;
extern bool isReadingWaveform;
extern int waveformReadIndex;
extern bool isReadingSpectrum;
extern int spectrumReadIndex;
extern Axis requestedAxis, lastCapturedAxis;
extern char lastCaptureTimestamp[21];
extern char lastAutoCaptureTimestamp[21];
extern int autoCaptureAxisIndex;
extern bool isAutoCaptureCycleActive;

// Button / alarm
extern unsigned long buttonPressStartTime;
extern bool apModeTriggeredByButton, wasWifiConnected, wifiDisconnectAlarmActive;
extern unsigned long lastAlarmActionTime;
extern bool isAlarmBeeping;

// Metrics
extern float accelRmsX, accelRmsY, accelRmsZ;
extern float accelPeakX, accelPeakY, accelPeakZ;
extern float accelRms2X, accelRms2Y, accelRms2Z;
extern float accelPeak2X, accelPeak2Y, accelPeak2Z;
extern float veloRmsX, veloRmsY, veloRmsZ;
extern float veloPeakX, veloPeakY, veloPeakZ;
extern float displRmsX, displRmsY, displRmsZ;
extern float displPeakX, displPeakY, displPeakZ;
extern float truePeakX, truePeakY, truePeakZ;
extern float crestFactorX, crestFactorY, crestFactorZ;
extern float stdDeviationX, stdDeviationY, stdDeviationZ;
extern float temperature;
extern float accelMetricRmsX, accelMetricRmsY, accelMetricRmsZ;
extern float accelMetricPeakX, accelMetricPeakY, accelMetricPeakZ;
extern float accelMetricRms2X, accelMetricRms2Y, accelMetricRms2Z;
extern float accelMetricPeak2X, accelMetricPeak2Y, accelMetricPeak2Z;
extern float veloWideRmsX, veloWideRmsY, veloWideRmsZ;
extern float veloWidePeakX, veloWidePeakY, veloWidePeakZ;
extern float displLowRmsX, displLowRmsY, displLowRmsZ;
extern float displLowPeakX, displLowPeakY, displLowPeakZ;

// Buffers
extern int16_t fullWaveform[WAVEFORM_TOTAL_POINTS];
extern bool newWaveformAvailable;
extern int16_t spectrumData[SPECTRUM_TOTAL_POINTS];
extern bool newSpectrumAvailable;
extern int16_t globalScratchBuffer[WAVEFORM_TOTAL_POINTS];
extern std::map<Axis, std::vector<SpectrumPeak>> xyzPeakDataCache;

// Metrics logger
extern MetricsSnapshot metricsLog[METRICS_LOG_SIZE];
extern bool isMetricsRecording;
extern int metricsLogIndex;
extern int metricsLogCount;

// Timers
extern unsigned long lastMetricsReadTime;
extern long metricsReadInterval_ms;
extern unsigned long lastCaptureActionTime, lastAutoCaptureMillis;
extern long dataCaptureInterval;
extern unsigned long ledBlinkTime;
extern bool ledState;
extern unsigned long lastPollTime;
extern int pollAttemptCounter;
extern unsigned long lastMqttAttemptTime;
extern unsigned long lastHealthPublishTime;
