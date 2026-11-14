#include <Arduino.h>
#include "platform/platform.h"
#include "config/board_pins.h"
#include "local_charts.h"
#include "ioc/ioc.h"
#include "core/sensor_policy.h"
#include "web/sensor_health_page.h"
#include "web/dashboard_page.h"
#include "web/network_page.h"
// --- Standard Arduino & ESP32 Libraries ---
#include <SPI.h>
#include <SD.h>
#ifndef FILE_APPEND
#define FILE_APPEND FILE_WRITE
#endif
#include <WiFi.h>
#if !GATEWAY_ETHERNET
#include <WiFiClientSecure.h>
#endif
#include <WebServer.h>
#include <WilcoxonMqtt.h>
#include <ArduinoJson.h>
#if !GATEWAY_ETHERNET
#include <Adafruit_NeoPixel.h>
#endif
#include "time.h"

// --- Modbus Libraries (RTU Master for Sensor & TCP Server for Client Access) ---
#include <WilcoxonILA210RTU.h> // For RS485 communication with the sensor
#include "platform/modbus_tcp.h"    // For exposing data over Wi-Fi (Modbus TCP Server)

// --- Standard C++ Libraries ---
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <new>
#include <atomic>
#include <errno.h>
#include <limits.h>

// --- FreeRTOS Concurrency Control ---
// FreeRTOS and the settings store are selected by platform/platform.h.

// Global Mutex for protecting shared data between RTOS tasks
SemaphoreHandle_t dataMutex;
SemaphoreHandle_t intervalSettingsMutex;
SemaphoreHandle_t sdMutex;
class SdLock {
public:
    SdLock() : locked(xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {}
    ~SdLock() { if (locked) xSemaphoreGive(sdMutex); }
    explicit operator bool() const { return locked; }
    SdLock(const SdLock&) = delete;
    SdLock& operator=(const SdLock&) = delete;
private:
    bool locked;
};

std::atomic<bool> mqttConnected{false};
bool dynamicStreamActive = false; // Protected by dataMutex.
uint32_t recordingGeneration = 0; // Invalidates baseline work after clear/restart.

// --- Storage Mode ---
enum StorageMode { STORAGE_NONE, STORAGE_SD, STORAGE_INTERNAL };
StorageMode currentStorageMode = STORAGE_NONE;

// --- Physical Relay Configuration ---
#if GATEWAY_ETHERNET
#define RELAY_ALERT_PIN PIN_DO0
#define RELAY_ALARM_PIN PIN_DO1
#else
#define RELAY_ALERT_PIN 1
#define RELAY_ALARM_PIN 2
#endif
bool relayAlertEnabled = false;
bool relayAlarmEnabled = false;
float veloPeakAlertThreshold = 10.0;
float veloPeakAlarmThreshold = 20.0;
unsigned long relayTriggerDelay = 5000; // 5 seconds default delay
unsigned long alertThresholdExceededTime = 0;
unsigned long alarmThresholdExceededTime = 0;
bool isAlertRelayActive = false;
bool isAlarmRelayActive = false;

// --- Onboard RGB NeoPixel Configuration ---
#if !GATEWAY_ETHERNET
#define RGB_PIN 46
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
#else
// IRIV has a monochrome user LED; keep blinking states without an RGB driver.
class StatusPixels {
public:
    void begin() { pinMode(PIN_LED, OUTPUT); }
    void setBrightness(int) {}
    uint32_t Color(int r, int g, int b) { return r | g | b; }
    void setPixelColor(int, uint32_t value) { active = value != 0; }
    void show() { digitalWrite(PIN_LED, active ? HIGH : LOW); }
private:
    bool active = false;
} pixels;
#endif

// --- SD Card Configuration ---
#define SD_CS_PIN 5
File metricsFile;
File peaksFile;
File eventFile;

// --- Device Identification & Firmware Version ---
const char* DEVICE_MODEL = "ILA";
const char* DEVICE_SERIAL_NUMBER = ""; // Unknown until verified from the correct ILA manual.
String gateway_id;
#ifndef ILA_RELEASE_VERSION
#define ILA_RELEASE_VERSION "1.5.1-dev"
#endif
const char* FIRMWARE_VERSION = ILA_RELEASE_VERSION;

// --- NTP Configuration ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; // JSON timestamps ending in Z must be UTC.
const int   daylightOffset_sec = 0;

// --- MQTT Configuration ---
String mqtt_server_str = ""; // set in Settings > MQTT
#if GATEWAY_ETHERNET
int mqtt_port_val = 1883;
#else
int mqtt_port_val = 8883;
#endif
String mqtt_user_str = "";
String mqtt_pass_str = ""; // never commit broker credentials
String mqtt_topic_str = "ILA";
String mqtt_command_topic = "ILA/command";
String mqtt_status_topic = "ILA/status";

// --- Wi-Fi & Web Server ---
WebServer server(80);
GatewayPreferences preferences;
#if !GATEWAY_ETHERNET
WiFiClientSecure espClientSecure;
#endif
WiFiClient espClientPlain;
WilcoxonMqttClient mqttClient(espClientPlain);
bool mqttTransportReady = false;
#if __has_include("local_mqtt_ca.h")
#include "local_mqtt_ca.h" // Defines ILA_MQTT_ROOT_CA as a PEM certificate string.
#endif

// --- Modbus RTU (RS485) Master Configuration ---
#if GATEWAY_ETHERNET
#define RXD_PIN PIN_SERIAL2_RX
#define TXD_PIN PIN_SERIAL2_TX
#define DE_RE_PIN -1
#define SENSOR_SERIAL Serial2
#else
#define RXD_PIN   16
#define TXD_PIN   18
#define DE_RE_PIN 10
#define SENSOR_SERIAL Serial1
#endif
uint8_t modbus_slave_id_val = 1;
long modbus_baud_rate_val = 115200;
WilcoxonILA210RTU node;

// --- Modbus TCP Server Configuration ---
bool modbusTcpEnabled = true; // Enable by default
uint16_t modbusTcpPort = 502; // Standard Modbus TCP port

// Define a structure for our Modbus register map
struct ModbusRegister {
  const char* name;
  const char* unit;
  float scale;
};

// Create the map of all registers
const ModbusRegister registerMap[] = {
  { "Accel RMS 1",     "g",      100.0 }, // Address 0
  { "Accel Peak 1",    "g",      100.0 }, // Address 1
  { "Accel RMS 2",     "g",      100.0 }, // Address 2
  { "Accel Peak 2",    "g",      100.0 }, // Address 3
  { "Velo RMS",        "mm/s",   10.0  }, // Address 4
  { "Velo Peak",       "mm/s",   10.0  }, // Address 5
  { "Displ RMS",       "um",     1.0   }, // Address 6
  { "Displ Peak",      "um",     1.0   }, // Address 7
  { "True Peak",       "g",      100.0 }, // Address 8
  { "Crest Factor",    "",       100.0 }, // Address 9
  { "Std Deviation",   "",      100.0 }, // Address 10
  { "Temperature",     "C",      10.0  }, // Address 11
  { "Drive Voltage",   "V",      100.0 }, // Address 12
  { "Board Temp",      "C",      10.0  }  // Address 13
};
const int numRegistersInMap = sizeof(registerMap) / sizeof(registerMap[0]);

WilcoxonILA210TCP modbusTCPServer;
uint32_t sensorConnectionGeneration = 0;
bool sensorSetupBusy = false; // Protected by dataMutex; UART owned by background task.
#define MODBUS_TCP_CACHE_SIZE 50
// Note: The cache size is larger than the map to allow for future expansion.

// --- Global Status & State Variables ---
char systemStatus[128] = "Initializing...";
enum DynamicDataState { DD_IDLE, DD_REQUEST_DATA, DD_WAITING_FOR_DATA };
DynamicDataState currentDataState = DD_IDLE;
bool isReadingWaveform = false;
int waveformReadIndex = 0;
bool isReadingSpectrum = false;
int spectrumReadIndex = 0;

// --- Global Sensor Metrics ---
gateway::SensorHealth sensorHealth; // Protected by dataMutex.
uint8_t sensorLossOutputMode = 0; // Hold, force enabled alarm ON, force outputs OFF.
bool alarmMetricValid = false;
bool metricsValid = false; // Protected by dataMutex; never present startup zeros as readings.
unsigned long lastMetricsSuccessAt = 0;
float accelRms1=0.0, accelPeak1=0.0, accelRms2=0.0, accelPeak2=0.0, veloRms=0.0, veloPeak=0.0, displRms=0.0, displPeak=0.0;
float truePeak=0.0, crestFactor=0.0, stdDeviation=0.0, temperature=0.0, driveVoltage=0.0, boardTemperature=0.0;

// --- Global Data Arrays ---
const uint16_t WAVEFORM_TOTAL_POINTS = 25600;
int16_t fullWaveform[WAVEFORM_TOTAL_POINTS];
bool newWaveformAvailable = false;
const uint16_t SPECTRUM_TOTAL_POINTS = 6401;
uint16_t spectrumData[SPECTRUM_TOTAL_POINTS];
bool newSpectrumAvailable = false;

// --- Spectrum Analysis & Peak Logger Variables ---
const int NUM_PEAKS_TO_TRACK = 4;
const int PEAK_HISTORY_SIZE = 5;
const float SPECTRUM_PEAK_THRESHOLD = 0.005;
const float ALARM_GROWTH_THRESHOLD = 1.25;
const float TREND_THRESHOLD_PERCENT = 0.02;
const float FREQ_RESOLUTION = 1.5625;
const float SPECTRUM_SCALE_FACTOR = 0.001;

// Detailed history entry
struct PeakHistoryEntry {
    char timestamp[21] = {0};
    float amplitude = 0.0;
};

struct SpectrumPeak {
    float frequency = 0.0;
    float amplitude = 0.0;
    String status = "➖ Stable";
    PeakHistoryEntry history[PEAK_HISTORY_SIZE]; // Use new detailed struct
    int historyCount = 0;
};
SpectrumPeak trackedPeaks[NUM_PEAKS_TO_TRACK];
SpectrumPeak previousPeaks[NUM_PEAKS_TO_TRACK];
bool baselineSpectrumCaptured = false;

const int PEAK_LOG_SIZE = 30;
const int BASELINE_SAMPLES_REQUIRED = 10;
const float ANOMALY_STD_DEV_THRESHOLD = 3.0;

struct LoggedPeak { float frequency = 0.0; float amplitude = 0.0; };
struct DataSnapshot { char timestamp[21] = {0}; LoggedPeak peaks[NUM_PEAKS_TO_TRACK]; bool is_valid = false; };
DataSnapshot peakLog[PEAK_LOG_SIZE];
int peakLogIndex = 0;
int peakLogCount = 0;

struct PeakBaseline { float frequency = 0.0; float mean = 0.0; float stdDev = 0.0; bool is_set = false; };
PeakBaseline baselines[NUM_PEAKS_TO_TRACK];

bool isPeakRecording = false;
bool isBaselineEstablished = false;
bool anomalyDetected = false;
int baselineSamplesCollected = 0;
String anomalyDetails = "";

// --- Metrics Logger Variables ---
const int METRICS_LOG_SIZE = 30;
struct MetricsSnapshot { char timestamp[21] = {0}; float values[14]; bool is_valid = false; };
MetricsSnapshot metricsLog[METRICS_LOG_SIZE];
bool isMetricsRecording = false;
int metricsLogIndex = 0;
int metricsLogCount = 0;

// --- Analysis Event Logger ---
const int EVENT_LOG_SIZE = 50;
struct AnalysisEvent { char timestamp[21] = {0}; float frequency = 0.0; char event_text[64] = {0}; bool is_valid = false; };
AnalysisEvent eventLog[EVENT_LOG_SIZE];
int eventLogIndex = 0;
int eventLogCount = 0;

// --- Timers & LED Control ---
unsigned long lastCaptureActionTime = 0;
long dataCaptureInterval = 600000;
const long INTERVAL_NEVER = 2147483647L;
unsigned long ledBlinkTime = 0;
bool ledState = false;
const long dataPollInterval = 500;
unsigned long lastPollTime = 0;
const int maxPollAttempts = 20;
int pollAttemptCounter = 0;
unsigned long lastMqttAttemptTime = 0;
const long mqttReconnectInterval = 5000;

// --- Function Prototypes ---
void handleRoot(); void handleMetrics(); void handleSpectrumAnalysis(); void handleFullWaveform(); void handleFullSpectrum(); void handleSettings(); void handleWifiSettings(); void handleSaveWifi(); void handleMqttSettings(); void handleSaveMqtt(); void handleRs485Settings(); void handleSaveRs485(); void handleTriggerWaveform(); void handleSetIntervals(); void handleGetIntervals(); void handleScanWifi(); void handleNotFound(); void analyzeSpectrum(); void handleGetLogData(); void handleDataLogControl(); void handleDownloadCsv(); void mqttCallback(char* topic, byte* payload, unsigned int length); void logAndAnalyzeData(SpectrumPeak currentPeaks[]); bool calculateBaseline(uint32_t generation); void checkForAnomalies(SpectrumPeak currentPeaks[], uint32_t generation); void publishAlert(String alertType, String details); void logMetrics(); void handleGetMetricsLogData(); void handleMetricsLogControl(); void handleDownloadMetricsCsv(); void handleClearPeakLog(); void handleClearMetricsLog(); void handleClearDynamicData(); void handleGetEventLog(); void handleClearEventLog(); void handleReboot();
void handleRelaySettings(); void handleSaveRelay(); void handleRelayLogic();
void handleDeviceInfo(); void handleModbusTcpSettings(); void handleSaveModbusTcp();
String formatUptime(unsigned long ms);


// =================================================================
// --- SECTION: UTILITY & CORE FUNCTIONS
// =================================================================
String formatUptime(unsigned long ms) {
    unsigned long seconds = ms / 1000;
    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;
    return String(days) + "d " + String(hours) + "h " + String(minutes) + "m";
}

float calculateLinearRegressionSlope(const PeakHistoryEntry history[], int n) {
    if (n < 2) return 0.0;
    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (int i = 0; i < n; i++) {
        sumX += i;
        sumY += history[i].amplitude;
        sumXY += i * history[i].amplitude;
        sumX2 += i * i;
    }
    float numerator = (float)n * sumXY - sumX * sumY;
    float denominator = (float)n * sumX2 - sumX * sumX;
    if (denominator == 0) return 0.0;
    return numerator / denominator;
}

String getTimestamp() {
  struct tm timeinfo;
#if GATEWAY_ETHERNET
  const time_t now = time(nullptr);
  if (now < 1577836800 || !gmtime_r(&now, &timeinfo)) return "1970-01-01T00:00:00Z";
#else
  if (!getLocalTime(&timeinfo, 0)) { return "1970-01-01T00:00:00Z"; }
#endif
  char timeString[21];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeString);
}

void updateLedStatus() {
    unsigned long currentMillis = millis();
    bool local_anomalyDetected = false;
    bool local_isReading = false;
    bool local_isRecording = false;
    bool local_isBaselineEstablished = false;
    char local_status[128] = "Status unavailable";

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        local_anomalyDetected = anomalyDetected;
        local_isReading = isReadingWaveform || isReadingSpectrum || currentDataState == DD_WAITING_FOR_DATA;
        local_isRecording = isPeakRecording;
        local_isBaselineEstablished = isBaselineEstablished;
        strncpy(local_status, systemStatus, sizeof(local_status));
        xSemaphoreGive(dataMutex);
    }

    if (local_anomalyDetected || isAlertRelayActive || isAlarmRelayActive) {
        int brightness = (int)(127.5 * (1 + sin(2 * PI * currentMillis / 2000.0)));
        pixels.setPixelColor(0, pixels.Color(brightness, 0, 0));
        pixels.show();
        return;
    }
    if (local_isReading) {
        if (currentMillis - ledBlinkTime > 500) {
            ledBlinkTime = currentMillis;
            ledState = !ledState;
            pixels.setPixelColor(0, ledState ? pixels.Color(255, 255, 0) : pixels.Color(0, 0, 0));
            pixels.show();
        }
        return;
    }
    if (local_isRecording && !local_isBaselineEstablished) {
        if (currentMillis - ledBlinkTime > 500) {
            ledBlinkTime = currentMillis;
            ledState = !ledState;
            pixels.setPixelColor(0, ledState ? pixels.Color(0, 255, 255) : pixels.Color(0, 0, 0));
            pixels.show();
        }
        return;
    }
    if (strstr(local_status, "failed") != NULL || strstr(local_status, "Error") != NULL || strstr(local_status, "Timeout") != NULL) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    } else if (!gw::netConnected()) {
        pixels.setPixelColor(0, pixels.Color(200, 200, 200));
    } else {
        pixels.setPixelColor(0, pixels.Color(0, 255, 0));
    }
    pixels.show();
}

void preTransmission() { if (DE_RE_PIN >= 0) digitalWrite(DE_RE_PIN, HIGH); }
void postTransmission() { if (DE_RE_PIN >= 0) digitalWrite(DE_RE_PIN, LOW); }

File openLogFile(const char* filename) {
    const size_t MAX_LOG_BYTES = 1024 * 1024;
    File existing = SD.open(filename, FILE_READ);
    const bool rotate = existing && existing.size() >= MAX_LOG_BYTES;
    existing.close();
    if (rotate) {
        String previous = String(filename) + ".previous";
        if (SD.exists(previous) && !SD.remove(previous)) return File();
        if (!SD.rename(filename, previous)) return File();
    }
    return SD.open(filename, FILE_APPEND);
}

void logEvent(float frequency, String eventText) {
    SdLock sd;
    if (!sd) return;
    const String ts = getTimestamp();
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    AnalysisEvent& event = eventLog[eventLogIndex];
    snprintf(event.timestamp, sizeof(event.timestamp), "%s", ts.c_str());
    snprintf(event.event_text, sizeof(event.event_text), "%s", eventText.c_str());
    event.frequency = frequency;
    event.is_valid = true;
    eventLogIndex = (eventLogIndex + 1) % EVENT_LOG_SIZE;
    if (eventLogCount < EVENT_LOG_SIZE) ++eventLogCount;
    xSemaphoreGive(dataMutex);
    if (currentStorageMode == STORAGE_SD) {
        File file = openLogFile("/event_log.csv");
        if (file) {
            eventText.replace("\"", "\"\"");
            file.print(ts); file.print(','); file.print(frequency, 2);
            file.print(",\""); file.print(eventText); file.println("\"");
            file.close();
        }
    }
}

// =================================================================
bool parseInteger(const String& text, long minimum, long maximum, long& value) {
    if (text.length() == 0 || text.length() > 10) return false;
    for (size_t i = 0; i < text.length(); ++i) if (text[i] < '0' || text[i] > '9') return false;
    errno = 0;
    char* end = nullptr;
    const long parsed = strtol(text.c_str(), &end, 10);
    if (errno || *end || parsed < minimum || parsed > maximum) return false;
    value = parsed;
    return true;
}

bool parseThreshold(const String& text, float& value) {
    if (text.isEmpty() || text.length() > 16) return false;
    char* end = nullptr;
    value = strtof(text.c_str(), &end);
    return end != text.c_str() && *end == '\0' && std::isfinite(value) && value >= 0 && value <= 3276.7f;
}

String htmlEscape(String text) {
    text.replace("&", "&amp;"); text.replace("<", "&lt;");
    text.replace(">", "&gt;"); text.replace("\"", "&quot;");
    text.replace("'", "&#39;");
    return text;
}

// --- SECTION: MQTT FUNCTIONS
// =================================================================

void publishMetrics() {
  if (!mqttClient.connected()) return;
  StaticJsonDocument<1024> doc;
  doc["model"] = DEVICE_MODEL;
  doc["serial_number"] = DEVICE_SERIAL_NUMBER;
  doc["gateway_id"] = gateway_id;
  doc["timestamp"] = getTimestamp();
  doc["type"] = "metrics";
  JsonObject data = doc.createNestedObject("data");

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    data["accelRms1"] = accelRms1; data["accelPeak1"] = accelPeak1;
    data["accelRms2"] = accelRms2; data["accelPeak2"] = accelPeak2;
    data["veloRms"] = veloRms; data["veloPeak"] = veloPeak;
    data["displRms"] = displRms; data["displPeak"] = displPeak;
    data["truePeak"] = truePeak; data["crestFactor"] = crestFactor;
    data["stdDeviation"] = stdDeviation;
    data["temperature"] = temperature; data["driveVoltage"] = driveVoltage;
    data["boardTemperature"] = boardTemperature;
    xSemaphoreGive(dataMutex);
  } else {
    return;
  }

  char payload[1024];
  if (doc.overflowed() || measureJson(doc) >= sizeof(payload)) {
      Serial.println("MQTT JSON exceeds capacity; publish skipped."); return;
  }
  size_t n = serializeJson(doc, payload, sizeof(payload));
  mqttClient.publish(mqtt_topic_str.c_str(), reinterpret_cast<const uint8_t*>(payload), n, false);
}

void publishDownsampledWaveform() {
  if (!mqttClient.connected()) return;

  StaticJsonDocument<3072> doc;
  doc["model"] = DEVICE_MODEL;
  doc["serial_number"] = DEVICE_SERIAL_NUMBER;
  doc["gateway_id"] = gateway_id;
  doc["timestamp"] = getTimestamp();
  doc["type"] = "waveform_preview";
  JsonArray data = doc.createNestedArray("data");
  const int DOWNSAMPLE_FACTOR = 200;
  const float scaleFactor = 0.001;

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (!newWaveformAvailable) {
        xSemaphoreGive(dataMutex);
        return;
    }
    for (int i = 0; i < WAVEFORM_TOTAL_POINTS; i += DOWNSAMPLE_FACTOR) {
      data.add(fullWaveform[i] * scaleFactor);
    }
    xSemaphoreGive(dataMutex);
  } else {
      return;
  }

  char payload[2048];
  if (doc.overflowed() || measureJson(doc) >= sizeof(payload)) {
      Serial.println("MQTT JSON exceeds capacity; publish skipped."); return;
  }
  size_t n = serializeJson(doc, payload, sizeof(payload));
  mqttClient.publish(mqtt_topic_str.c_str(), reinterpret_cast<const uint8_t*>(payload), n, false);
}

int waveformPublishIndex = -1; // Background task only.
String waveformCaptureId;

void publishFullWaveformInChunks() {
    if (!mqttClient.connected()) return;
    waveformPublishIndex = 0;
    waveformCaptureId = String(millis());
}

void serviceWaveformPublish() {
    if (waveformPublishIndex < 0) return;
    if (!mqttClient.connected()) { waveformPublishIndex = -1; return; }
    const int POINTS_PER_CHUNK = 80; // Fits the JSON document and MQTT packet.
    StaticJsonDocument<2048> doc;
    doc["model"] = DEVICE_MODEL;
    doc["serial_number"] = DEVICE_SERIAL_NUMBER;
  doc["gateway_id"] = gateway_id;
    doc["timestamp"] = getTimestamp();
    doc["type"] = "waveform_chunk";
    doc["capture_id"] = waveformCaptureId;
    doc["chunk_index"] = waveformPublishIndex / POINTS_PER_CHUNK;
    doc["total_chunks"] = (WAVEFORM_TOTAL_POINTS + POINTS_PER_CHUNK - 1) / POINTS_PER_CHUNK;
    JsonArray data = doc.createNestedArray("data");
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    const int end = min(waveformPublishIndex + POINTS_PER_CHUNK, (int)WAVEFORM_TOTAL_POINTS);
    for (int i = waveformPublishIndex; i < end; ++i) data.add(fullWaveform[i] * 0.001f);
    xSemaphoreGive(dataMutex);
    char payload[2048];
    if (doc.overflowed() || measureJson(doc) >= sizeof(payload)) {
        Serial.println("Waveform MQTT document overflow; transfer aborted.");
        waveformPublishIndex = -1;
        return;
    }
    const size_t n = serializeJson(doc, payload, sizeof(payload));
    if (!mqttClient.publish(mqtt_topic_str.c_str(), reinterpret_cast<const uint8_t*>(payload), n)) {
        Serial.println("Waveform MQTT publish failed; transfer aborted.");
        waveformPublishIndex = -1;
        return;
    }
    waveformPublishIndex = end < WAVEFORM_TOTAL_POINTS ? end : -1;
}

void publishAlert(String alertType, String details) {
    if (!mqttClient.connected()) return;
    StaticJsonDocument<512> doc;
    doc["model"] = DEVICE_MODEL;
    doc["serial_number"] = DEVICE_SERIAL_NUMBER;
  doc["gateway_id"] = gateway_id;
    doc["timestamp"] = getTimestamp();
    doc["type"] = "alert";
    doc["alert_type"] = alertType;
    doc["details"] = details.substring(0, 240);
    char payload[512];
    if (doc.overflowed() || measureJson(doc) >= sizeof(payload)) {
        Serial.println("MQTT JSON exceeds capacity; publish skipped."); return;
    }
    serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(mqtt_status_topic.c_str(), payload);
}

void publishStatus() {
    if (!mqttClient.connected()) return;
    StaticJsonDocument<512> doc;
    doc["model"] = DEVICE_MODEL;
    doc["serial_number"] = DEVICE_SERIAL_NUMBER;
  doc["gateway_id"] = gateway_id;
    doc["timestamp"] = getTimestamp();
    doc["type"] = "status_update";

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        doc["status_text"] = String(systemStatus);
        xSemaphoreGive(dataMutex);
    } else {
        doc["status_text"] = "Status unavailable (mutex locked)";
    }

    char payload[512];
    if (doc.overflowed() || measureJson(doc) >= sizeof(payload)) {
        Serial.println("MQTT JSON exceeds capacity; publish skipped."); return;
    }
    serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(mqtt_status_topic.c_str(), payload);
}

bool saveCaptureInterval(long interval) {
    xSemaphoreTake(intervalSettingsMutex, portMAX_DELAY);
    GatewayPreferences settings;
    bool saved = settings.begin("device-cfg", false);
    if (saved) { settings.putLong("wf_interval", interval); saved = settings.commit(); settings.end(); }
    if (saved) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        dataCaptureInterval = interval;
        xSemaphoreGive(dataMutex);
    }
    xSemaphoreGive(intervalSettingsMutex);
    return saved;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    StaticJsonDocument<256> doc;
    if (strcmp(topic, mqtt_command_topic.c_str()) != 0 ||
        deserializeJson(doc, payload, length)) return;

    const char* target = doc["gateway_id"];
    if (gateway::commandTargetsGateway(target, gateway_id.c_str())) {
        const char* command = doc["command"];
        if (command) {
            Serial.printf("Received command for this device: %s\n", command);
            if (strcmp(command, "set_interval") == 0) {
                const long interval = doc["interval_ms"] | 0L;
                const bool saved = interval >= 1000 && saveCaptureInterval(interval);
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    snprintf(systemStatus, sizeof(systemStatus), saved ? "Interval saved" : "Interval save failed; unchanged");
                    xSemaphoreGive(dataMutex);
                }
                return;
            }
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                if (strcmp(command, "start_recording") == 0) {
                    isPeakRecording = true;
                    ++recordingGeneration;
                    peakLogIndex = 0; peakLogCount = 0; baselineSamplesCollected = 0;
                    isBaselineEstablished = false; anomalyDetected = false;
                    for(int i=0; i<PEAK_LOG_SIZE; ++i) peakLog[i].is_valid = false;
                    snprintf(systemStatus, sizeof(systemStatus), "Recording started (Learning)");
                } else if (strcmp(command, "stop_recording") == 0) {
                    isPeakRecording = false;
                    ++recordingGeneration;
                    snprintf(systemStatus, sizeof(systemStatus), "Recording stopped");
                    anomalyDetected = false;
                } else if (strcmp(command, "trigger_capture") == 0) {
                    if (!sensorSetupBusy && currentDataState == DD_IDLE && !isReadingWaveform &&
                        !isReadingSpectrum && !dynamicStreamActive) currentDataState = DD_REQUEST_DATA;
                } else if (strcmp(command, "get_status") == 0) {
                    // Status is published outside the lock
                }
                xSemaphoreGive(dataMutex);
            }

            if (strcmp(command, "reboot") == 0) {
                gw::restart();
            } else if (strcmp(command, "get_status") == 0) {
                publishStatus();
            }
        }
    }
}

// =================================================================
// --- SECTION: MODBUS & SENSOR FUNCTIONS
// =================================================================

bool readSensorValue(const char* metricName, uint16_t address, float scale_factor, float& value) {
  uint8_t result = node.readInputRegisters(address, 1);
  if (result == node.ku8MBSuccess) {
    value = node.getResponseBuffer(0) * scale_factor;
    return true;
  } else {
    Serial.printf("DEBUG: FAILED - Reading %s. Error: 0x%X\n", metricName, result);
    return false;
  }
}

// Reverted to original waveform trigger method
bool requestNewWaveform() {
    uint8_t result = node.writeSingleRegister(0x0200, 0xFFFF);
    return (result == node.ku8MBSuccess);
}

// Reverted to original waveform ready check
bool isWaveformReady() {
    uint8_t result = node.readHoldingRegisters(0x0200, 1);
    if (result == node.ku8MBSuccess) {
        return (node.getResponseBuffer(0) == 0x0000);
    }
    return false;
}

void analyzeSpectrum() {
    Serial.println("Analyzing new spectrum data for peaks...");

    SpectrumPeak newTrackedPeaks[NUM_PEAKS_TO_TRACK];
    for (auto& peak : newTrackedPeaks) {
        peak.amplitude = -1.0;
    }

    uint16_t* localSpectrumData = new (std::nothrow) uint16_t[SPECTRUM_TOTAL_POINTS];
    if (localSpectrumData == nullptr) {
      Serial.println("Failed to allocate memory for spectrum analysis!");
      return;
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        memcpy(localSpectrumData, spectrumData, SPECTRUM_TOTAL_POINTS * sizeof(uint16_t));
        xSemaphoreGive(dataMutex);
    } else {
        Serial.println("AnalyzeSpectrum failed to get mutex");
        delete[] localSpectrumData;
        return;
    }

    for (int i = 1; i < SPECTRUM_TOTAL_POINTS - 1; i++) {
        float amp = abs(localSpectrumData[i] * SPECTRUM_SCALE_FACTOR);
        if (amp > SPECTRUM_PEAK_THRESHOLD) {
            float prevAmp = abs(localSpectrumData[i-1] * SPECTRUM_SCALE_FACTOR);
            float nextAmp = abs(localSpectrumData[i+1] * SPECTRUM_SCALE_FACTOR);

            if (amp > prevAmp && amp >= nextAmp) {
                if (amp > newTrackedPeaks[NUM_PEAKS_TO_TRACK - 1].amplitude) {
                    newTrackedPeaks[NUM_PEAKS_TO_TRACK - 1].frequency = (float)i * FREQ_RESOLUTION;
                    newTrackedPeaks[NUM_PEAKS_TO_TRACK - 1].amplitude = amp;

                    std::sort(std::begin(newTrackedPeaks), std::end(newTrackedPeaks), [](const SpectrumPeak& a, const SpectrumPeak& b){
                        return a.amplitude > b.amplitude;
                    });
                }
            }
        }
    }

    delete[] localSpectrumData;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < NUM_PEAKS_TO_TRACK; i++) {
            if (newTrackedPeaks[i].frequency == 0) continue;
            bool foundMatch = false;

            for (int j = 0; j < NUM_PEAKS_TO_TRACK; j++) {
                if (previousPeaks[j].frequency > 0 && abs(previousPeaks[j].frequency - newTrackedPeaks[i].frequency) < (FREQ_RESOLUTION * 2)) {
                    newTrackedPeaks[i].historyCount = previousPeaks[j].historyCount;
                    memcpy(newTrackedPeaks[i].history, previousPeaks[j].history, sizeof(previousPeaks[j].history));
                    foundMatch = true;
                    break;
                }
            }

            // Updated history logging with timestamps
            if (newTrackedPeaks[i].historyCount < PEAK_HISTORY_SIZE) {
                // Just add to the end
                String ts = getTimestamp();
                strncpy(newTrackedPeaks[i].history[newTrackedPeaks[i].historyCount].timestamp, ts.c_str(), 20);
                newTrackedPeaks[i].history[newTrackedPeaks[i].historyCount].amplitude = newTrackedPeaks[i].amplitude;
                newTrackedPeaks[i].historyCount++;
            } else {
                // Shift history entries
                for(int k = 0; k < PEAK_HISTORY_SIZE - 1; k++) {
                    newTrackedPeaks[i].history[k] = newTrackedPeaks[i].history[k+1];
                }
                // Add new entry at the end
                String ts = getTimestamp();
                strncpy(newTrackedPeaks[i].history[PEAK_HISTORY_SIZE - 1].timestamp, ts.c_str(), 20);
                newTrackedPeaks[i].history[PEAK_HISTORY_SIZE - 1].amplitude = newTrackedPeaks[i].amplitude;
            }

            if (!foundMatch) {
                newTrackedPeaks[i].status = "✨ New";
            } else if (newTrackedPeaks[i].historyCount < 3) {
                newTrackedPeaks[i].status = "➖ Stable";
            } else {
                float currentVal = newTrackedPeaks[i].history[newTrackedPeaks[i].historyCount - 1].amplitude;
                float prevVal = newTrackedPeaks[i].history[newTrackedPeaks[i].historyCount - 2].amplitude;

                if (currentVal > prevVal * ALARM_GROWTH_THRESHOLD) {
                    newTrackedPeaks[i].status = "🚨 ALARM";
                } else {
                    float slope = calculateLinearRegressionSlope(newTrackedPeaks[i].history, newTrackedPeaks[i].historyCount);
                    float sum = 0;
                    for(int k=0; k < newTrackedPeaks[i].historyCount; ++k) sum += newTrackedPeaks[i].history[k].amplitude;
                    float mean = sum / newTrackedPeaks[i].historyCount;

                    if (mean > 0 && slope > (mean * TREND_THRESHOLD_PERCENT)) {
                        newTrackedPeaks[i].status = "📈 Growing";
                    } else if (mean > 0 && slope < -(mean * TREND_THRESHOLD_PERCENT)) {
                        newTrackedPeaks[i].status = "📉 Shrinking";
                    } else {
                        newTrackedPeaks[i].status = "➖ Stable";
                    }
                }
            }

            // logEvent acquires dataMutex; emit events after releasing this lock.
        }

        const bool recording = isPeakRecording;
        xSemaphoreGive(dataMutex);
        for (const auto& peak : newTrackedPeaks) {
            if (peak.frequency > 0) {
                logEvent(peak.frequency, "Reading - Status: " + peak.status +
                         ", Amp: " + String(peak.amplitude, 4) + "g");
            }
        }
        if (recording) {
            logAndAnalyzeData(newTrackedPeaks);
        } else {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                anomalyDetected = false;
                xSemaphoreGive(dataMutex);
            }
        }

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < NUM_PEAKS_TO_TRACK; ++i) {
                trackedPeaks[i] = newTrackedPeaks[i];
                previousPeaks[i] = newTrackedPeaks[i];
            }
            baselineSpectrumCaptured = true;
            xSemaphoreGive(dataMutex);
        }

    } else {
        Serial.println("AnalyzeSpectrum failed to get mutex for history check");
    }
    Serial.println("Spectrum analysis complete.");
}

void handleLongReads() {
  bool local_isReadingWaveform, local_isReadingSpectrum;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    local_isReadingWaveform = isReadingWaveform;
    local_isReadingSpectrum = isReadingSpectrum;
    xSemaphoreGive(dataMutex);
  } else { return; }


  if (local_isReadingWaveform) {
    char status_buffer[128];
    snprintf(status_buffer, sizeof(status_buffer), "Step 1/2: Collecting waveform (%d%%)", (waveformReadIndex * 100) / WAVEFORM_TOTAL_POINTS);
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      strncpy(systemStatus, status_buffer, sizeof(systemStatus));
      xSemaphoreGive(dataMutex);
    }

    const uint16_t START_ADDRESS = 0x1F01;
    const uint8_t POINTS_PER_CHUNK = 64; // ModbusMaster response-buffer capacity.
    uint16_t currentAddress = START_ADDRESS + waveformReadIndex;
    uint8_t pointsToRead = min((int)POINTS_PER_CHUNK, (int)(WAVEFORM_TOTAL_POINTS - waveformReadIndex));
    uint8_t result = node.readInputRegisters(currentAddress, pointsToRead);

    if (result == node.ku8MBSuccess) {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int j = 0; j < pointsToRead; j++) {
          fullWaveform[waveformReadIndex + j] = node.getResponseBuffer(j);
        }
        waveformReadIndex += pointsToRead;
        if (waveformReadIndex >= WAVEFORM_TOTAL_POINTS) {
          isReadingWaveform = false; isReadingSpectrum = true; spectrumReadIndex = 0; newWaveformAvailable = true;
          Serial.println("DEBUG: [WAVEFORM] Read complete.");
          xSemaphoreGive(dataMutex);
          if(mqttClient.connected()) { publishDownsampledWaveform(); publishFullWaveformInChunks(); }
        } else {
          xSemaphoreGive(dataMutex);
        }
      }
    } else {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        isReadingWaveform = false;
        snprintf(systemStatus, sizeof(systemStatus), "Waveform capture failed");
        lastCaptureActionTime = millis(); currentDataState = DD_IDLE;
        xSemaphoreGive(dataMutex);
      }
    }
    return;
  }

  if (local_isReadingSpectrum) {
    char status_buffer[128];
    snprintf(status_buffer, sizeof(status_buffer), "Step 2/2: Collecting spectrum (%d%%)", (spectrumReadIndex * 100) / SPECTRUM_TOTAL_POINTS);
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      strncpy(systemStatus, status_buffer, sizeof(systemStatus));
      xSemaphoreGive(dataMutex);
    }

    const uint16_t START_ADDRESS = 0x0600;
    const uint8_t POINTS_PER_CHUNK = 64; // ModbusMaster response-buffer capacity.
    uint16_t currentAddress = START_ADDRESS + spectrumReadIndex;
    uint8_t pointsToRead = min((int)POINTS_PER_CHUNK, (int)(SPECTRUM_TOTAL_POINTS - spectrumReadIndex));
    uint8_t result = node.readInputRegisters(currentAddress, pointsToRead);

    if (result == node.ku8MBSuccess) {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int j = 0; j < pointsToRead; j++) {
          spectrumData[spectrumReadIndex + j] = node.getResponseBuffer(j);
        }
        spectrumReadIndex += pointsToRead;
        if (spectrumReadIndex >= SPECTRUM_TOTAL_POINTS) {
          isReadingSpectrum = false; newSpectrumAvailable = true;
          snprintf(systemStatus, sizeof(systemStatus), "Data capture complete");
          Serial.println("DEBUG: [SPECTRUM] Read complete.");
          lastCaptureActionTime = millis(); currentDataState = DD_IDLE;
          xSemaphoreGive(dataMutex);
          analyzeSpectrum();
        } else {
            xSemaphoreGive(dataMutex);
        }
      }
    } else {
      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        isReadingSpectrum = false;
        snprintf(systemStatus, sizeof(systemStatus), "Spectrum capture failed");
        lastCaptureActionTime = millis(); currentDataState = DD_IDLE;
        xSemaphoreGive(dataMutex);
      }
    }
  }
}

void logAndAnalyzeData(SpectrumPeak currentPeaks[]) {
    SdLock sd;
    if (!sd) return;
    bool local_isPeakRecording;
    uint32_t generation;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!isPeakRecording) { xSemaphoreGive(dataMutex); return; }
        generation = recordingGeneration;
        DataSnapshot* snapshot = &peakLog[peakLogIndex];
        String ts = getTimestamp();
        strncpy(snapshot->timestamp, ts.c_str(), sizeof(snapshot->timestamp) - 1);
        for(int i = 0; i < NUM_PEAKS_TO_TRACK; i++) {
            snapshot->peaks[i].frequency = currentPeaks[i].frequency;
            snapshot->peaks[i].amplitude = currentPeaks[i].amplitude;
        }
        snapshot->is_valid = true;
        peakLogIndex = (peakLogIndex + 1) % PEAK_LOG_SIZE;
        if (peakLogCount < PEAK_LOG_SIZE) {
            peakLogCount++;
        }
        local_isPeakRecording = isPeakRecording;
        xSemaphoreGive(dataMutex);
    } else { return; }

    if (local_isPeakRecording && currentStorageMode == STORAGE_SD) {
        peaksFile = openLogFile("/peak_log.csv");
        if (peaksFile) {
            String ts = getTimestamp();
            peaksFile.print(ts);
            for(int i = 0; i < NUM_PEAKS_TO_TRACK; i++) {
                peaksFile.print(",");
                peaksFile.print(currentPeaks[i].frequency, 2);
                peaksFile.print(",");
                peaksFile.print(currentPeaks[i].amplitude, 4);
            }
            peaksFile.println();
            peaksFile.close();
        } else {
            Serial.println("Error opening peak_log.csv for writing");
        }
    }

    bool local_isBaselineEstablished;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        local_isBaselineEstablished = isBaselineEstablished;
        xSemaphoreGive(dataMutex);
    } else { return; }

    if (!local_isBaselineEstablished) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (generation != recordingGeneration || !isPeakRecording) {
                xSemaphoreGive(dataMutex); return;
            }
            bool hasPeak = false;
            for (int i = 0; i < NUM_PEAKS_TO_TRACK; ++i) hasPeak |= currentPeaks[i].frequency > 0;
            if (!hasPeak) { xSemaphoreGive(dataMutex); return; }
            baselineSamplesCollected++;
            snprintf(systemStatus, sizeof(systemStatus), "Learning Baseline (%d/%d)", baselineSamplesCollected, BASELINE_SAMPLES_REQUIRED);
            if (baselineSamplesCollected >= BASELINE_SAMPLES_REQUIRED) {
                xSemaphoreGive(dataMutex);
                if (calculateBaseline(generation)) {
                  publishAlert("info", "Baseline established. Anomaly monitoring is now active.");
                }
            } else {
                xSemaphoreGive(dataMutex);
            }
        }
    } else {
        checkForAnomalies(currentPeaks, generation);
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (generation != recordingGeneration || !isPeakRecording) { xSemaphoreGive(dataMutex); return; }
            if (anomalyDetected) {
                snprintf(systemStatus, sizeof(systemStatus), "ANOMALY DETECTED!");
            } else {
                snprintf(systemStatus, sizeof(systemStatus), "Recording & Monitoring Anomaly");
            }
            xSemaphoreGive(dataMutex);
        }
    }
}

bool calculateBaseline(uint32_t generation) {
    Serial.println("Calculating baseline for anomaly detection...");
    DataSnapshot localPeakLog[PEAK_LOG_SIZE];
    int localPeakLogCount;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        memcpy(localPeakLog, peakLog, sizeof(peakLog));
        localPeakLogCount = peakLogCount;
        if (generation != recordingGeneration || !isPeakRecording) {
            xSemaphoreGive(dataMutex); return false;
        }
        xSemaphoreGive(dataMutex);
    } else { return false; }

    PeakBaseline newBaselines[NUM_PEAKS_TO_TRACK];

    for (int i = 0; i < NUM_PEAKS_TO_TRACK; ++i) {
        std::vector<float> amplitudes;
        float targetFreq = 0;
        for(int j=0; j < localPeakLogCount; ++j) {
            if (localPeakLog[j].is_valid && localPeakLog[j].peaks[i].frequency > 0) {
                targetFreq = localPeakLog[j].peaks[i].frequency;
                break;
            }
        }
        if (targetFreq == 0) continue;

        for (int j = 0; j < localPeakLogCount; ++j) {
            if (localPeakLog[j].is_valid) {
                for (int k = 0; k < NUM_PEAKS_TO_TRACK; ++k) {
                    if (abs(localPeakLog[j].peaks[k].frequency - targetFreq) < (FREQ_RESOLUTION * 5)) {
                        amplitudes.push_back(localPeakLog[j].peaks[k].amplitude);
                        break;
                    }
                }
            }
        }
        if (amplitudes.size() > 1) {
            float sum = std::accumulate(amplitudes.begin(), amplitudes.end(), 0.0);
            float mean = sum / amplitudes.size();
            float variance = 0.0f;
            for (float amplitude : amplitudes) {
                const float delta = amplitude - mean;
                variance += delta * delta;
            }
            float stdDev = std::sqrt(variance / amplitudes.size());
            newBaselines[i].frequency = targetFreq;
            newBaselines[i].mean = mean;
            newBaselines[i].stdDev = stdDev;
            newBaselines[i].is_set = true;
            Serial.printf("Baseline for ~%.2f Hz: Mean=%.4f, StdDev=%.4f\n", targetFreq, mean, stdDev);
        }
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool valid = false;
        for (const auto& baseline : newBaselines) valid |= baseline.is_set;
        if (generation != recordingGeneration || !isPeakRecording || !valid) {
            xSemaphoreGive(dataMutex); return false;
        }
        memcpy(baselines, newBaselines, sizeof(baselines));
        isBaselineEstablished = true;
        snprintf(systemStatus, sizeof(systemStatus), "Recording & Monitoring Anomaly");
        xSemaphoreGive(dataMutex);
        return true;
    }
    return false;
}

void checkForAnomalies(SpectrumPeak currentPeaks[], uint32_t generation) {
    bool detected = false;
    String details = "";
    PeakBaseline localBaselines[NUM_PEAKS_TO_TRACK];

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(localBaselines, baselines, sizeof(baselines));
        xSemaphoreGive(dataMutex);
    } else { return; }

    for (int i = 0; i < NUM_PEAKS_TO_TRACK; ++i) {
        if (currentPeaks[i].frequency > 0) {
            for (int j = 0; j < NUM_PEAKS_TO_TRACK; ++j) {
                if (localBaselines[j].is_set && abs(currentPeaks[i].frequency - localBaselines[j].frequency) < (FREQ_RESOLUTION * 5)) {
                    float threshold = localBaselines[j].mean + ANOMALY_STD_DEV_THRESHOLD * localBaselines[j].stdDev;
                    if (currentPeaks[i].amplitude > threshold) {
                        detected = true;
                        details += "Peak at " + String(currentPeaks[i].frequency, 2) + "Hz is " + String(currentPeaks[i].amplitude, 4) + "g (Threshold: " + String(threshold, 4) + "g). ";
                    }
                    break;
                }
            }
        }
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (generation != recordingGeneration || !isPeakRecording) { xSemaphoreGive(dataMutex); return; }
        anomalyDetected = detected;
        anomalyDetails = details;
        xSemaphoreGive(dataMutex);
        if (detected) publishAlert("anomaly", details);
    }
}

void logMetrics() {
    SdLock sd;
    if (!sd) return;
    if(!isMetricsRecording) return;

    float localMetrics[14];
    String ts = getTimestamp();

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        MetricsSnapshot* snapshot = &metricsLog[metricsLogIndex];
        strncpy(snapshot->timestamp, ts.c_str(), sizeof(snapshot->timestamp) - 1);
        snapshot->values[0] = accelRms1; snapshot->values[1] = accelPeak1; snapshot->values[2] = accelRms2; snapshot->values[3] = accelPeak2;
        snapshot->values[4] = veloRms;   snapshot->values[5] = veloPeak; snapshot->values[6] = displRms;  snapshot->values[7] = displPeak;
        snapshot->values[8] = truePeak;  snapshot->values[9] = crestFactor; snapshot->values[10] = stdDeviation; snapshot->values[11] = temperature;
        snapshot->values[12] = driveVoltage; snapshot->values[13] = boardTemperature;
        snapshot->is_valid = true;
        metricsLogIndex = (metricsLogIndex + 1) % METRICS_LOG_SIZE;
        if (metricsLogCount < METRICS_LOG_SIZE) {
            metricsLogCount++;
        }
        memcpy(localMetrics, snapshot->values, sizeof(localMetrics));
        xSemaphoreGive(dataMutex);
    } else { return; }

    if (currentStorageMode == STORAGE_SD) {
        metricsFile = openLogFile("/metrics_log.csv");
        if (metricsFile) {
            metricsFile.print(ts);
            for(int v=0; v<14; ++v) {
                metricsFile.print(",");
                metricsFile.print(localMetrics[v]);
            }
            metricsFile.println();
            metricsFile.close();
        } else {
            Serial.println("Error opening metrics_log.csv");
        }
    }
}

void handleRelayLogic() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { return; }
    const bool alarmWasActive = isAlarmRelayActive;
    const bool alertWasActive = isAlertRelayActive;
    if (relayAlarmEnabled) {
        if (veloPeak > veloPeakAlarmThreshold) {
            if (alarmThresholdExceededTime == 0) {
                alarmThresholdExceededTime = millis();
            } else if (millis() - alarmThresholdExceededTime >= relayTriggerDelay) {
                if (!isAlarmRelayActive) {
                    digitalWrite(RELAY_ALARM_PIN, HIGH);
                    isAlarmRelayActive = true;

                }
            }
        } else {
            if (isAlarmRelayActive) {
                digitalWrite(RELAY_ALARM_PIN, LOW);
                isAlarmRelayActive = false;

            }
            alarmThresholdExceededTime = 0;
        }
    } else {
        alarmThresholdExceededTime = 0;
        if (isAlarmRelayActive) {
            digitalWrite(RELAY_ALARM_PIN, LOW);
            isAlarmRelayActive = false;
        }
    }

    if (relayAlertEnabled) {
        if (veloPeak > veloPeakAlertThreshold && !isAlarmRelayActive) {
            if (alertThresholdExceededTime == 0) {
                alertThresholdExceededTime = millis();
            } else if (millis() - alertThresholdExceededTime >= relayTriggerDelay) {
                if (!isAlertRelayActive) {
                    digitalWrite(RELAY_ALERT_PIN, HIGH);
                    isAlertRelayActive = true;

                }
            }
        } else {
            if (isAlertRelayActive) {
                digitalWrite(RELAY_ALERT_PIN, LOW);
                isAlertRelayActive = false;

            }
            alertThresholdExceededTime = 0;
        }
    } else {
        alertThresholdExceededTime = 0;
        if (isAlertRelayActive) {
            digitalWrite(RELAY_ALERT_PIN, LOW);
            isAlertRelayActive = false;
        }
    }

    const bool alarmNowActive = isAlarmRelayActive;
    const bool alertNowActive = isAlertRelayActive;
    xSemaphoreGive(dataMutex);
    if (alarmWasActive != alarmNowActive)
        publishAlert(alarmNowActive ? "RELAY_ACTIVE" : "RELAY_INACTIVE", alarmNowActive ? "ALARM relay triggered." : "ALARM relay reset.");
    if (alertWasActive != alertNowActive)
        publishAlert(alertNowActive ? "RELAY_ACTIVE" : "RELAY_INACTIVE", alertNowActive ? "ALERT relay triggered." : "ALERT relay reset.");
}

// =================================================================
// --- MODBUS TCP SERVER DATA UPDATE FUNCTION
// =================================================================

/**
 * @brief Copies the latest sensor metrics into the Modbus TCP server's holding registers.
 * This function uses the globally defined registerMap to ensure consistency.
 */
void updateModbusTCPCache() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Create a temporary array of the current metric values in the correct order
        float metrics[] = {
            accelRms1, accelPeak1, accelRms2, accelPeak2,
            veloRms, veloPeak, displRms, displPeak,
            truePeak, crestFactor, stdDeviation,
            temperature, driveVoltage, boardTemperature
        };

        // Loop through our register map and update the Modbus server's holding registers
        for (int i = 0; i < numRegistersInMap; i++) {
            // Scale the float value to an integer and write it to the register
            const long raw = std::lround(metrics[i] * registerMap[i].scale);
            const int16_t encoded = static_cast<int16_t>(std::max(-32768L, std::min(32767L, raw)));
            modbusTCPServer.Hreg(i, static_cast<uint16_t>(encoded));
        }

        xSemaphoreGive(dataMutex);
    }
}


// =================================================================
// --- RTOS TASKS ---
// =================================================================

void webServerLoop(void * pvParameters) {
  Serial.println("Web Server Task running on core 0");
  for(;;) {
    server.handleClient();
    vTaskDelay(2);
  }
}

void syncSensorToIoc() {
    gateway::SensorHealth health;
    bool alert, alarm;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
    health = sensorHealth; alert = isAlertRelayActive; alarm = isAlarmRelayActive;
    xSemaphoreGive(dataMutex);
    ioc::setSensorAlarms(alert, alarm, health);
}

void applySensorLossPolicy() {
    static bool previousFault = false;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
    const bool fault = sensorHealth.lost(millis());
    if (fault) {
        alertThresholdExceededTime = alarmThresholdExceededTime = 0;
        if (sensorLossOutputMode == 1 && relayAlarmEnabled) {
            digitalWrite(RELAY_ALARM_PIN, HIGH); isAlarmRelayActive = true;
        } else if (sensorLossOutputMode == 2) {
            digitalWrite(RELAY_ALERT_PIN, LOW); digitalWrite(RELAY_ALARM_PIN, LOW);
            isAlertRelayActive = isAlarmRelayActive = false;
        }
    }
    xSemaphoreGive(dataMutex);
    syncSensorToIoc();
    if (fault != previousFault) {
        previousFault = fault;
        publishAlert("sensor_communication", fault ? "Sensor communication timeout" : "Sensor communication restored");
    }
}

bool serviceSensorRtu() {
    static gateway::SensorSchedule schedule;
    static float pending[14] = {};
    static uint32_t generation = 0;
    const uint16_t addresses[] = {0x0000,0x0004,0x0010,0x0014,0x0020,0x0024,0x0030,0x0034,0x0040,0x0044,0x0048,0x0050,0x0060,0x007C};
    const float scales[] = {.01f,.01f,.01f,.01f,.1f,.1f,1,1,.01f,.01f,.01f,.1f,.01f,.1f};
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) != pdTRUE) return true;
    if (generation != sensorConnectionGeneration) { generation = sensorConnectionGeneration; schedule = gateway::SensorSchedule(); memset(pending, 0, sizeof(pending)); }
    const bool capturing = isReadingWaveform || isReadingSpectrum;
    xSemaphoreGive(dataMutex);
    const gateway::RtuJob job = schedule.next(millis(), capturing);
    if (job == gateway::RtuJob::Alarm) {
        float value = 0;
        const bool ok = readSensorValue("Velocity Peak alarm", 0x0024, .1f, value);
        schedule.complete(job, millis(), ok);
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            alarmMetricValid = ok;
            if (ok) {
                veloPeak = value; sensorHealth.seen = true; sensorHealth.lastSuccessAt = millis();
            } else alertThresholdExceededTime = alarmThresholdExceededTime = 0;
            xSemaphoreGive(dataMutex);
        }
        if (ok) { handleRelayLogic(); updateModbusTCPCache(); }
        syncSensorToIoc();
    } else if (job == gateway::RtuJob::Metrics) {
        const uint8_t index = schedule.metricIndex();
        const bool ok = readSensorValue(registerMap[index].name, addresses[index], scales[index], pending[index]);
        const bool finished = schedule.complete(job, millis(), ok);
        if (!ok || finished) {
            bool recording = false;
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return true;
            metricsValid = ok && finished;
            if (metricsValid) {
                lastMetricsSuccessAt = millis();
                accelRms1=pending[0]; accelPeak1=pending[1]; accelRms2=pending[2]; accelPeak2=pending[3];
                veloRms=pending[4]; // veloPeak is owned by the newer priority alarm sample.
                displRms=pending[6]; displPeak=pending[7]; truePeak=pending[8]; crestFactor=pending[9];
                stdDeviation=pending[10]; temperature=pending[11]; driveVoltage=pending[12]; boardTemperature=pending[13];
                recording = isMetricsRecording;
            } else if (!capturing) snprintf(systemStatus, sizeof(systemStatus), "Error reading metrics");
            xSemaphoreGive(dataMutex);
            if (ok && finished) { publishMetrics(); if (recording) logMetrics(); updateModbusTCPCache(); }
        }
    } else if (job == gateway::RtuJob::Capture) handleLongReads();
    return job != gateway::RtuJob::None;
}

void handleSensorHealth() {
    if (server.method() == HTTP_POST) {
        StaticJsonDocument<256> body;
        if (server.arg("plain").length() > 256 || deserializeJson(body, server.arg("plain")) ||
            !body["timeout_ms"].is<uint32_t>() || body["timeout_ms"].as<uint32_t>() < 2000 ||
            body["timeout_ms"].as<uint32_t>() > 600000 || !body["fault_alarm"].is<bool>() ||
            !body["output_mode"].is<uint8_t>() || body["output_mode"].as<uint8_t>() > 2) {
            server.send(400, "text/plain", "Invalid sensor-loss configuration."); return;
        }
        GatewayPreferences settings;
        bool saved = settings.begin("sensor-loss", false);
        if (saved) {
            settings.putUInt("timeout_ms", body["timeout_ms"]);
            settings.putBool("fault_alarm", body["fault_alarm"]);
            settings.putUChar("output_mode", body["output_mode"]);
            saved = settings.commit(); settings.end();
        }
        if (!saved) { server.send(503, "text/plain", "Settings could not be stored; no changes applied."); return; }
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        sensorHealth.timeoutMs = body["timeout_ms"];
        sensorHealth.faultAlarmEnabled = body["fault_alarm"];
        sensorLossOutputMode = body["output_mode"];
        xSemaphoreGive(dataMutex);
        syncSensorToIoc();
    }
    StaticJsonDocument<512> doc;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    doc["timeout_ms"] = sensorHealth.timeoutMs; doc["fault_alarm"] = sensorHealth.faultAlarmEnabled;
    doc["output_mode"] = sensorLossOutputMode; doc["sensor_fault"] = sensorHealth.lost(millis());
    doc["sample_seen"] = sensorHealth.seen;
    xSemaphoreGive(dataMutex);
    doc["gateway_id"] = gateway_id;
    String body; serializeJson(doc, body); server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", body);
}

#if GATEWAY_ETHERNET
// Ethernet address mode: DHCP (default) or a static address saved in the eth-cfg namespace.
gw::EthernetAddress loadEthernetAddress() {
    gw::EthernetAddress address;
    GatewayPreferences settings;
    if (!settings.begin("eth-cfg", true)) return address;
    const bool wanted = settings.getBool("static", false);
    const bool parsed = gw::parseIPv4(settings.getString("ip"), address.ip, false) &&
                        gw::parseIPv4(settings.getString("mask"), address.mask, false) &&
                        gw::parseIPv4(settings.getString("gw"), address.gateway, true) &&
                        gw::parseIPv4(settings.getString("dns"), address.dns, true);
    settings.end();
    String error;
    address.useStatic = wanted && parsed && gw::ethernetCheckStatic(address, error);
    if (wanted && !address.useStatic) Serial.println("Saved static IP is invalid (" + (parsed ? error : String("unreadable address")) + "); using DHCP");
    return address;
}

void handleNetworkApi() {
    if (server.method() == HTTP_POST) {
        StaticJsonDocument<256> body;
        if (server.arg("plain").length() > 256 || deserializeJson(body, server.arg("plain"))) {
            server.send(400, "text/plain", "Invalid network settings."); return;
        }
        const String mode = body["mode"] | "";
        if (mode != "dhcp" && mode != "static") { server.send(400, "text/plain", "Mode must be dhcp or static."); return; }
        gw::EthernetAddress address;
        address.useStatic = mode == "static";
        if (address.useStatic) {
            String error;
            if (!gw::parseIPv4(body["ip"] | "", address.ip, false)) { server.send(400, "text/plain", "Enter a valid IP address."); return; }
            if (!gw::parseIPv4(body["mask"] | "", address.mask, false)) { server.send(400, "text/plain", "Enter a valid subnet mask."); return; }
            if (!gw::parseIPv4(body["gateway"] | "", address.gateway, true)) { server.send(400, "text/plain", "Enter a valid gateway or leave it empty."); return; }
            if (!gw::parseIPv4(body["dns"] | "", address.dns, true)) { server.send(400, "text/plain", "Enter a valid DNS server or leave it empty."); return; }
            if (!gw::ethernetCheckStatic(address, error)) { server.send(400, "text/plain", error + "."); return; }
        }
        const IPAddress none(0, 0, 0, 0);
        GatewayPreferences settings;
        bool saved = settings.begin("eth-cfg", false);
        if (saved) {
            settings.putBool("static", address.useStatic);
            settings.putString("ip", address.useStatic ? address.ip.toString() : String());
            settings.putString("mask", address.useStatic ? address.mask.toString() : String());
            settings.putString("gw", address.useStatic && address.gateway != none ? address.gateway.toString() : String());
            settings.putString("dns", address.useStatic && address.dns != none ? address.dns.toString() : String());
            saved = settings.commit(); settings.end();
        }
        if (!saved) { server.send(503, "text/plain", "Settings could not be saved; device was not rebooted."); return; }
        server.send(200, "text/plain", address.useStatic ? "Static IP saved. The gateway restarts now." : "DHCP selected. The gateway restarts now.");
        delay(1000);
        gw::restart();
        return;
    }
    StaticJsonDocument<768> doc;
    GatewayPreferences settings;
    const bool opened = settings.begin("eth-cfg", true);
    doc["mode"] = opened && settings.getBool("static", false) ? "static" : "dhcp";
    doc["ip"] = settings.getString("ip");
    doc["mask"] = settings.getString("mask");
    doc["gateway"] = settings.getString("gw");
    doc["dns"] = settings.getString("dns");
    settings.end();
    doc["fallback_ip"] = ETHERNET_FALLBACK_IP;
    JsonObject current = doc.createNestedObject("current");
    current["link"] = gw::netConnected();
    current["description"] = gw::netDescription();
    current["ip"] = gw::netIP().toString();
    current["mask"] = gw::ethernetMask().toString();
    current["gateway"] = gw::ethernetGateway().toString();
    current["dns"] = gw::ethernetDns().toString();
    current["mac"] = gw::ethernetMac();
    current["hostname"] = ETHERNET_HOSTNAME;
    String json; serializeJson(doc, json);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}
#endif

#include "web/sensor_setup.h"

void backgroundLoop(void * pvParameters) {
  Serial.println("Background Task (Modbus, MQTT, AI) running on core 1");
  gw::watchdogStart();
  for(;;) {
      gw::watchdogFeed();
      const bool setupUsed = serviceSensorSetup();
      const bool sensorBusUsed = setupUsed || serviceSensorRtu();
      applySensorLossPolicy();
#if GATEWAY_ETHERNET
      static uint32_t buttonPressedAt = 0;
      static bool fallbackRequested = false;
      if (digitalRead(USER_BUTTON_PIN) == LOW) {
          if (!buttonPressedAt) buttonPressedAt = millis();
          if (!fallbackRequested && millis() - buttonPressedAt >= 5000) {
              gw::ethernetUseFallback();
              fallbackRequested = true;
          }
      } else { buttonPressedAt = 0; fallbackRequested = false; }
#endif
      if (gw::netConnected() && mqttTransportReady && mqtt_server_str.length() > 0) {
        if (!mqttClient.connected()) {
            if (millis() - lastMqttAttemptTime > mqttReconnectInterval) {
                lastMqttAttemptTime = millis();
                Serial.print("Attempting MQTT connection...");
                if (mqttClient.connect((String("ila-") + gateway_id).c_str(), mqtt_user_str.c_str(), mqtt_pass_str.c_str())) {
                    Serial.println("connected");
                    mqttClient.subscribe(mqtt_command_topic.c_str());
                } else {
                    Serial.print("failed, rc="); Serial.print(mqttClient.state()); Serial.println(" try again in 5 seconds");
                }
            }
        }
        if (mqttClient.connected()) {
            mqttClient.loop();
            ioc::serviceMqtt(mqttClient, mqtt_topic_str);
        }
      }

      mqttConnected.store(gw::netConnected() && mqttClient.connected());

      // Handle the Modbus TCP Server if it's enabled
      if (modbusTcpEnabled) {
          modbusTCPServer.task();
      }

      if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          if (!relayAlarmEnabled) { digitalWrite(RELAY_ALARM_PIN, LOW); isAlarmRelayActive = false; alarmThresholdExceededTime = 0; }
          if (!relayAlertEnabled) { digitalWrite(RELAY_ALERT_PIN, LOW); isAlertRelayActive = false; alertThresholdExceededTime = 0; }
          xSemaphoreGive(dataMutex);
      }
      syncSensorToIoc();
      updateLedStatus();
      serviceWaveformPublish();

      bool local_isReadingWaveform, local_isReadingSpectrum;
      if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          local_isReadingWaveform = isReadingWaveform;
          local_isReadingSpectrum = isReadingSpectrum;
          xSemaphoreGive(dataMutex);
      } else { continue; }


      if (!local_isReadingWaveform && !local_isReadingSpectrum && !sensorBusUsed) {
        DynamicDataState local_currentDataState;
        long local_dataCaptureInterval;
        unsigned long local_lastCaptureActionTime;

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            local_currentDataState = currentDataState;
            local_dataCaptureInterval = dataCaptureInterval;
            local_lastCaptureActionTime = lastCaptureActionTime;
            xSemaphoreGive(dataMutex);
        } else { continue; }

        switch (local_currentDataState) {
          case DD_IDLE:
            if (local_dataCaptureInterval > 0 && local_dataCaptureInterval < INTERVAL_NEVER && (millis() - local_lastCaptureActionTime >= local_dataCaptureInterval)) {
              if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (!sensorSetupBusy) currentDataState = DD_REQUEST_DATA;
                xSemaphoreGive(dataMutex);
              }
            }
            break;
          case DD_REQUEST_DATA:
            if (waveformPublishIndex >= 0) break;
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                snprintf(systemStatus, sizeof(systemStatus), "Requesting new data...");
                xSemaphoreGive(dataMutex);
            }
            if (requestNewWaveform()) {
              if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                pollAttemptCounter = 0; lastPollTime = millis(); currentDataState = DD_WAITING_FOR_DATA;
                xSemaphoreGive(dataMutex);
              }
            } else {
              if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                snprintf(systemStatus, sizeof(systemStatus), "Failed to request data");
                lastCaptureActionTime = millis(); currentDataState = DD_IDLE;
                xSemaphoreGive(dataMutex);
              }
            }
            break;
          case DD_WAITING_FOR_DATA:
             if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                snprintf(systemStatus, sizeof(systemStatus), "Waiting for sensor...");
                xSemaphoreGive(dataMutex);
            }
            if (millis() - lastPollTime >= dataPollInterval) {
              lastPollTime = millis();
              if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                pollAttemptCounter++;
                xSemaphoreGive(dataMutex);
              }
              if (isWaveformReady()) {
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                  if (dynamicStreamActive) { xSemaphoreGive(dataMutex); break; }
                  isReadingWaveform = true; waveformReadIndex = 0; newWaveformAvailable = false; newSpectrumAvailable = false;
                  xSemaphoreGive(dataMutex);
                }
              } else if (pollAttemptCounter >= maxPollAttempts) {
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                  snprintf(systemStatus, sizeof(systemStatus), "Timeout waiting for sensor");
                  lastCaptureActionTime = millis(); currentDataState = DD_IDLE;
                  xSemaphoreGive(dataMutex);
                }
              }
            }
            break;
        }
      }
      vTaskDelay(10);
  }
}

// =================================================================
// --- SETUP & MAIN LOOP ---
// =================================================================

bool requestIocCapture() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    const bool available = !sensorSetupBusy && currentDataState == DD_IDLE && !isReadingWaveform && !isReadingSpectrum && !dynamicStreamActive;
    if (available) currentDataState = DD_REQUEST_DATA;
    xSemaphoreGive(dataMutex);
    return available;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    gw::platformBegin();
    gateway_id = gw::deviceId();
#if GATEWAY_ETHERNET
    pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
#endif
    dataMutex = xSemaphoreCreateMutex();
    intervalSettingsMutex = xSemaphoreCreateMutex();
    sdMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL || sdMutex == NULL || intervalSettingsMutex == NULL) {
        Serial.println("Error: Could not create data mutex");
        gw::restart();
    }

    pixels.begin();
    pixels.setBrightness(40);
    pixels.setPixelColor(0, pixels.Color(0, 0, 255));
    pixels.show();

    pinMode(RELAY_ALERT_PIN, OUTPUT);
    pinMode(RELAY_ALARM_PIN, OUTPUT);
    digitalWrite(RELAY_ALERT_PIN, LOW);
    digitalWrite(RELAY_ALARM_PIN, LOW);

#if GATEWAY_ETHERNET
    const bool sdReady = false; // IRIV has no onboard SD; never drive its input GPIO5 as CS.
#else
    const bool sdReady = SD.begin(SD_CS_PIN);
#endif
    if(!sdReady){
        Serial.println("Card Mount Failed. Switching to internal memory logging.");
        currentStorageMode = STORAGE_INTERNAL;
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            snprintf(systemStatus, sizeof(systemStatus), "SD Error. Using internal memory.");
            xSemaphoreGive(dataMutex);
        }
    } else {
        Serial.println("SD card initialized.");
        currentStorageMode = STORAGE_SD;
    }

    preferences.begin("device-cfg", true); dataCaptureInterval = preferences.getLong("wf_interval", 600000); preferences.end();
    preferences.begin("mqtt-cfg", true); mqtt_server_str = preferences.getString("server", mqtt_server_str); mqtt_port_val = preferences.getInt("port", mqtt_port_val); mqtt_user_str = preferences.getString("user", mqtt_user_str); mqtt_pass_str = preferences.getString("pass", mqtt_pass_str); mqtt_topic_str = preferences.getString("topic", mqtt_topic_str); preferences.end();
    mqtt_command_topic = mqtt_topic_str + "/command";
    mqtt_status_topic = mqtt_topic_str + "/status";
    preferences.begin("rs485-cfg", true); modbus_slave_id_val = preferences.getUChar("slave_id", modbus_slave_id_val); modbus_baud_rate_val = preferences.getLong("baud_rate", modbus_baud_rate_val); preferences.end();
    preferences.begin("relay-cfg", true);
        relayAlertEnabled = preferences.getBool("alert_en", false);
        relayAlarmEnabled = preferences.getBool("alarm_en", false);
        veloPeakAlertThreshold = preferences.getFloat("alert_thr", 10.0);
        veloPeakAlarmThreshold = preferences.getFloat("alarm_thr", 20.0);
        relayTriggerDelay = preferences.getULong("delay", 5000);
    preferences.end();
    preferences.begin("modbustcp-cfg", true);
        modbusTcpEnabled = preferences.getBool("enabled", true);
        modbusTcpPort = preferences.getUShort("port", 502);
    preferences.end();

    if (preferences.begin("sensor-loss", true)) {
        sensorHealth.timeoutMs = preferences.getUInt("timeout_ms", 10000);
        sensorHealth.faultAlarmEnabled = preferences.getBool("fault_alarm", true);
        sensorLossOutputMode = preferences.getUChar("output_mode", 0);
        preferences.end();
    }
    if (sensorHealth.timeoutMs < 2000 || sensorHealth.timeoutMs > 600000) sensorHealth.timeoutMs = 10000;
    if (sensorLossOutputMode > 2) sensorLossOutputMode = 0;
    if (dataCaptureInterval < 1000) dataCaptureInterval = 600000;
    if (modbus_slave_id_val < 1 || modbus_slave_id_val > 247) modbus_slave_id_val = 1;
    if (modbus_baud_rate_val != 9600 && modbus_baud_rate_val != 19200 && modbus_baud_rate_val != 38400 && modbus_baud_rate_val != 57600 && modbus_baud_rate_val != 115200) modbus_baud_rate_val = 115200;
    if (mqtt_port_val < 1 || mqtt_port_val > 65535) mqtt_port_val = 8883;
    if (modbusTcpPort == 0 || modbusTcpPort == 80) modbusTcpPort = 502;
    if (!std::isfinite(veloPeakAlertThreshold) || veloPeakAlertThreshold < 0) veloPeakAlertThreshold = 10;
    if (!std::isfinite(veloPeakAlarmThreshold) || veloPeakAlarmThreshold < veloPeakAlertThreshold) veloPeakAlarmThreshold = std::max(20.0f, veloPeakAlertThreshold);
    if (relayTriggerDelay > 3600000) relayTriggerDelay = 5000;
#if GATEWAY_ETHERNET
    const gw::EthernetAddress ethAddress = loadEthernetAddress();
    const bool connected_to_wifi = gw::ethernetBegin(ethAddress); // wired connectivity, retained name below
#else
    String saved_ssid = "", saved_pass = "";
    preferences.begin("wifi-creds", true); saved_ssid = preferences.getString("ssid", ""); saved_pass = preferences.getString("password", ""); preferences.end();
    bool connected_to_wifi = false;
    if (saved_ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
        int attempts = 0;
        while (!gw::netConnected() && attempts < 30) { delay(500); attempts++; }
        if (gw::netConnected()) { connected_to_wifi = true; }
        else { WiFi.disconnect(true); }
    }
    if (!connected_to_wifi) { WiFi.mode(WIFI_AP); WiFi.softAP("ILA_Sensor_AP", "12345678"); }

#endif

#if GATEWAY_ETHERNET
    SENSOR_SERIAL.setRX(RXD_PIN);
    SENSOR_SERIAL.setTX(TXD_PIN);
    SENSOR_SERIAL.begin(modbus_baud_rate_val, SERIAL_8N1);
#else
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);
    SENSOR_SERIAL.begin(modbus_baud_rate_val, SERIAL_8N1, RXD_PIN, TXD_PIN);
#endif
    node.begin(modbus_slave_id_val, SENSOR_SERIAL, modbus_baud_rate_val);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    sensorHealth.startedAt = millis();

    if (modbusTcpEnabled) {
      modbusTcpEnabled = modbusTCPServer.addHreg(0, 0, MODBUS_TCP_CACHE_SIZE) && modbusTCPServer.server(modbusTcpPort);
      if (!modbusTcpEnabled) Serial.println("Modbus TCP startup failed");
      Serial.printf("Modbus TCP Server started on port %d\n", modbusTcpPort);
    } else if (connected_to_wifi) {
      Serial.println("Modbus TCP Server is disabled by configuration.");
    }

    if (!ioc::begin(requestIocCapture)) {
        Serial.println("IOC sampler initialization failed; restarting with outputs off.");
        delay(1000); gw::restart();
    }
    ioc::registerRoutes(server);
    server.on("/sensor_fault_settings", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", SENSOR_HEALTH_PAGE); });
    server.on("/sensor_setup", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", SENSOR_SETUP_PAGE); });
    server.on("/api/sensor_setup", HTTP_GET, handleSensorSetup);
    server.on("/api/sensor_setup", HTTP_POST, handleSensorSetup);
    server.on("/api/sensor_health", HTTP_GET, handleSensorHealth);
    server.on("/api/sensor_health", HTTP_POST, handleSensorHealth);
    server.on("/charts.js", HTTP_GET, []() { server.send_P(200, "application/javascript", LOCAL_CHARTS_JS); });
    server.on("/", HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleMetrics);
    server.on("/spectrum_analysis", HTTP_GET, handleSpectrumAnalysis);
    server.on("/fullwaveform", HTTP_GET, handleFullWaveform);
    server.on("/fullspectrum", HTTP_GET, handleFullSpectrum);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/wifi_settings", HTTP_GET, handleWifiSettings);
    server.on("/savewifi", HTTP_POST, handleSaveWifi);
#if GATEWAY_ETHERNET
    server.on("/api/network", HTTP_GET, handleNetworkApi);
    server.on("/api/network", HTTP_POST, handleNetworkApi);
#endif
    server.on("/mqtt_settings", HTTP_GET, handleMqttSettings);
    server.on("/save_mqtt", HTTP_POST, handleSaveMqtt);
    server.on("/rs485_settings", HTTP_GET, handleRs485Settings);
    server.on("/save_rs485", HTTP_GET, handleSaveRs485);
    server.on("/relay_settings", HTTP_GET, handleRelaySettings);
    server.on("/save_relay", HTTP_GET, handleSaveRelay);
    server.on("/trigger_waveform", HTTP_GET, handleTriggerWaveform);
    server.on("/set_intervals", HTTP_GET, handleSetIntervals);
    server.on("/get_intervals", HTTP_GET, handleGetIntervals);
    server.on("/scanwifi", HTTP_GET, handleScanWifi);
    server.on("/get_log_data", HTTP_GET, handleGetLogData);
    server.on("/datalog_control", HTTP_GET, handleDataLogControl);
    server.on("/download_csv", HTTP_GET, handleDownloadCsv);
    server.on("/clear_peak_log", HTTP_GET, handleClearPeakLog);
    server.on("/get_metrics_log_data", HTTP_GET, handleGetMetricsLogData);
    server.on("/metrics_log_control", HTTP_GET, handleMetricsLogControl);
    server.on("/download_metrics_csv", HTTP_GET, handleDownloadMetricsCsv);
    server.on("/clear_metrics_log", HTTP_GET, handleClearMetricsLog);
    server.on("/clear_dynamic_data", HTTP_GET, handleClearDynamicData);
    server.on("/get_event_log", HTTP_GET, handleGetEventLog);
    server.on("/clear_event_log", HTTP_GET, handleClearEventLog);
    server.on("/reboot", HTTP_GET, handleReboot);
    server.on("/device_info", HTTP_GET, handleDeviceInfo);
    server.on("/modbustcp_settings", HTTP_GET, handleModbusTcpSettings);
    server.on("/save_modbustcp", HTTP_GET, handleSaveModbusTcp);
    server.onNotFound(handleNotFound);
    server.begin();

    gw::startTimeSync(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (mqtt_port_val == 1883) {
        mqttClient.setClient(espClientPlain);
        mqttTransportReady = true; // Explicit plain MQTT port.
    } else {
#if defined(ILA_MQTT_ROOT_CA) && !GATEWAY_ETHERNET
        mqttClient.setClient(espClientSecure);
        espClientSecure.setCACert(ILA_MQTT_ROOT_CA);
        espClientSecure.setHandshakeTimeout(5);
        mqttTransportReady = true;
#else
        Serial.println("TLS MQTT unavailable: ESP32-S3 needs a root CA; IRIV uses plain MQTT port 1883.");
#endif
    }
    mqttClient.setServer(mqtt_server_str.c_str(), mqtt_port_val);
    mqttTransportReady = mqttClient.setBufferSize(4096) && mqttTransportReady;
    mqttClient.setSocketTimeout(2);
    mqttClient.setCallback(mqttCallback);

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        if (currentStorageMode != STORAGE_INTERNAL) {
           snprintf(systemStatus, sizeof(systemStatus), "Monitoring");
        }
        xSemaphoreGive(dataMutex);
    }

    if (!gw::startTask(webServerLoop, "WebServerTask", 12000, 1, 0) ||
        !gw::startTask(backgroundLoop, "BackgroundTask", 16384, 1, 1)) {
        Serial.println("Task allocation failed; restarting.");
        delay(1000);
        gw::restart();
    }
}

void loop() { vTaskDelete(NULL); }

// =================================================================
// --- SECTION: WEB SERVER HANDLERS
// =================================================================

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", DASHBOARD_PAGE);
}

void handleMetrics() {
  StaticJsonDocument<1024> doc;

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(250)) == pdTRUE) {
    doc["accelRms1"] = accelRms1; doc["accelPeak1"] = accelPeak1; doc["accelRms2"] = accelRms2;
    doc["accelPeak2"] = accelPeak2; doc["veloRms"] = veloRms; doc["veloPeak"] = veloPeak;
    doc["displRms"] = displRms; doc["displPeak"] = displPeak; doc["truePeak"] = truePeak;
    doc["crestFactor"] = crestFactor; doc["stdDeviation"] = stdDeviation; doc["temperature"] = temperature;
    doc["driveVoltage"] = driveVoltage; doc["boardTemperature"] = boardTemperature;
    doc["status"] = String(systemStatus);
    doc["gateway_id"] = gateway_id;
    doc["sensor_fault"] = sensorHealth.lost(millis());
    doc["alarm_sample_valid"] = alarmMetricValid;
    doc["metrics_valid"] = metricsValid;
    if (metricsValid) doc["metrics_age_ms"] = millis() - lastMetricsSuccessAt;
    doc["capture_busy"] = sensorSetupBusy || currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum;
    doc["newWaveformAvailable"] = newWaveformAvailable; doc["newSpectrumAvailable"] = newSpectrumAvailable;
    doc["alert_relay_active"] = isAlertRelayActive;
    doc["alarm_relay_active"] = isAlarmRelayActive;
    doc["alert_relay_enabled"] = relayAlertEnabled;
    doc["alarm_relay_enabled"] = relayAlarmEnabled;
    xSemaphoreGive(dataMutex);
  } else {
    server.send(503, "application/json", "{\"error\":\"System busy\"}"); return;
  }

  doc["mqtt_connected"] = mqttConnected.load();
  // Static identity for the dashboard header; const char* values are stored by pointer.
  doc["model"] = DEVICE_MODEL;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["network"] = gw::netDescription();
  doc["ioc"] = (bool)GATEWAY_ETHERNET;
  doc["sd_supported"] = !GATEWAY_ETHERNET;

  if (currentStorageMode == STORAGE_SD) {
      doc["storage_mode"] = "SD Card";
  } else {
      doc["storage_mode"] = "Internal (Limited)";
  }
  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void handleSpectrumAnalysis() {
    StaticJsonDocument<4096> doc; // Four peaks with five history entries each.
    JsonArray peaks = doc.createNestedArray("peaks");

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      for (int i = 0; i < NUM_PEAKS_TO_TRACK; i++) {
          if (trackedPeaks[i].frequency > 0) {
              JsonObject peak = peaks.createNestedObject();
              peak["frequency"] = trackedPeaks[i].frequency;
              peak["amplitude"] = trackedPeaks[i].amplitude;
              peak["status"] = trackedPeaks[i].status;

              // Serialize detailed history
              JsonArray history = peak.createNestedArray("history");
              for (int h = 0; h < trackedPeaks[i].historyCount; h++) {
                  JsonObject histEntry = history.createNestedObject();
                  histEntry["ts"] = String(trackedPeaks[i].history[h].timestamp);
                  histEntry["amp"] = trackedPeaks[i].history[h].amplitude;
              }
          }
      }
      xSemaphoreGive(dataMutex);
    }

    String payload;
    serializeJson(doc, payload);
    server.send(200, "application/json", payload);
}

void handleFullWaveform() {
    bool available = false;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        available = newWaveformAvailable && !isReadingWaveform && !isReadingSpectrum;
        if (available) dynamicStreamActive = true;
        xSemaphoreGive(dataMutex);
    }
    if (!available) { server.send(200, "application/json", "[]"); return; }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "[");

    const float scaleFactor = 0.001;
    const int CHUNK_SIZE = 128;
    int16_t buffer[CHUNK_SIZE];
    char num_buffer[16];

    for (int i = 0; i < WAVEFORM_TOTAL_POINTS; i += CHUNK_SIZE) {
        if (!server.client().connected()) {
            Serial.println("Client disconnected during waveform stream. Aborting.");
            break;
        }

        int pointsToCopy = min(CHUNK_SIZE, (int)(WAVEFORM_TOTAL_POINTS - i));

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            memcpy(buffer, &fullWaveform[i], pointsToCopy * sizeof(int16_t));
            xSemaphoreGive(dataMutex);
        } else {
            server.client().stop();
            break; // Abort incomplete JSON instead of returning invented zero samples.
        }

        String chunk = "";
        for (int j = 0; j < pointsToCopy; j++) {
            dtostrf(buffer[j] * scaleFactor, 1, 3, num_buffer);
            chunk += num_buffer;
            if (i + j < WAVEFORM_TOTAL_POINTS - 1) {
                chunk += ",";
            }
        }
        server.sendContent(chunk);
        vTaskDelay(1);
    }
    server.sendContent("]");
    server.sendContent(""); // Finish the HTTP chunked response.
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    dynamicStreamActive = false;
    xSemaphoreGive(dataMutex);
}

void handleFullSpectrum() {
    bool available = false;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        available = newSpectrumAvailable && !isReadingWaveform && !isReadingSpectrum;
        if (available) dynamicStreamActive = true;
        xSemaphoreGive(dataMutex);
    }
    if (!available) { server.send(200, "application/json", "{}"); return; }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");

    const float scaleFactor = 0.001;
    const float freq_resolution = 1.5625;
    const int CHUNK_SIZE = 128;
    uint16_t buffer[CHUNK_SIZE];
    char num_buffer[20];

    server.sendContent("{\"points\":[");
    for (int i = 0; i < SPECTRUM_TOTAL_POINTS; i += CHUNK_SIZE) {
        if (!server.client().connected()) { break; }
        int pointsToCopy = min(CHUNK_SIZE, (int)(SPECTRUM_TOTAL_POINTS - i));

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            memcpy(buffer, &spectrumData[i], pointsToCopy * sizeof(uint16_t));
            xSemaphoreGive(dataMutex);
        } else {
            server.client().stop();
            break; // Abort incomplete JSON instead of returning invented zero samples.
        }

        String chunk = "";
        for (int j = 0; j < pointsToCopy; j++) {
            dtostrf(abs(buffer[j] * scaleFactor), 1, 4, num_buffer);
            chunk += num_buffer;
            if (i + j < SPECTRUM_TOTAL_POINTS - 1) { chunk += ","; }
        }
        server.sendContent(chunk);
        vTaskDelay(1);
    }

    server.sendContent("],\"labels\":[");
    for (int i = 0; i < SPECTRUM_TOTAL_POINTS; i += CHUNK_SIZE) {
        if (!server.client().connected()) { break; }
        String chunk = "";
        int pointsToProcess = min(CHUNK_SIZE, (int)(SPECTRUM_TOTAL_POINTS - i));
        for (int j = 0; j < pointsToProcess; j++) {
            dtostrf((i + j) * freq_resolution, 1, 4, num_buffer);
            chunk += num_buffer;
            if (i + j < SPECTRUM_TOTAL_POINTS - 1) { chunk += ","; }
        }
        server.sendContent(chunk);
        vTaskDelay(1);
    }
    server.sendContent("]}");
    server.sendContent(""); // Finish the HTTP chunked response.
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    dynamicStreamActive = false;
    xSemaphoreGive(dataMutex);
}

void handleSettings() {
  String html;
  html += R"rawliteral(
<!DOCTYPE html><html><head><title>Device Settings</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px;box-sizing:border-box}.container{width:100%;max-width:500px;background-color:white;border-radius:12px;box-shadow:0 8px 30px rgba(0,0,0,.08);overflow:hidden}.header{text-align:center;padding:20px;border-bottom:1px solid #e9ecef}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.settings-list{list-style:none;padding:0;margin:0}.settings-item{display:flex;align-items:center;padding:15px 20px;border-bottom:1px solid #e9ecef;text-decoration:none;color:inherit;transition:background-color .2s}.settings-item:hover{background-color:#f8f9fa}.settings-item:last-child{border-bottom:none}.icon{width:24px;height:24px;margin-right:20px;color:#007bff}.content{flex-grow:1;text-align:left}.content h3{margin:0;font-size:16px;font-weight:500;color:#343a40}.content p{margin:2px 0 0;font-size:14px;color:#6c757d;display:flex;align-items:center}.chevron{width:20px;height:20px;color:#ced4da}.status-dot{height:9px;width:9px;border-radius:50%;display:inline-block;margin-right:6px}.status-dot.connected{background-color:#2ecc71}.status-dot.disconnected{background-color:#e74c3c}.actions{padding:20px;background-color:#f8f9fa;border-top:1px solid #e9ecef}.btn{display:block;width:100%;padding:12px;border:none;border-radius:8px;cursor:pointer;font-size:15px;font-weight:600;text-align:center;text-decoration:none;box-sizing:border-box;transition:background-color .2s,color .2s}.btn-danger{background-color:#e74c3c;color:white;margin-bottom:10px}.btn-danger:hover{background-color:#c0392b}.btn-secondary{background-color:transparent;color:#007bff}.btn-secondary:hover{background-color:#e9ecef}</style></head><body><div class="container"><div class="header"><h2>Device Settings</h2></div><div class="settings-list">
<a href="/wifi_settings" class="settings-item"><div class="icon"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M8.288 15.043A5.242 5.242 0 017.5 14.25a5.25 5.25 0 010-10.5 5.25 5.25 0 017.5 7.5c0 1.28-.466 2.466-1.243 3.393m-1.243 3.393a5.25 5.25 0 01-7.5-7.5" /></svg></div><div class="content"><h3>Wi-Fi</h3><p><span class="status-dot )rawliteral";
  html += (gw::netConnected() ? "connected" : "disconnected");
  html += R"rawliteral("></span>)rawliteral";
  html += htmlEscape(gw::netDescription());
  html += R"rawliteral(</p></div><div class="chevron"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M8.25 4.5l7.5 7.5-7.5 7.5" /></svg></div></a>
<a href="/mqtt_settings" class="settings-item"><div class="icon"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M12 16.5V9.75m0 0l3 3m-3-3l-3 3M6.75 19.5a4.5 4.5 0 01-1.41-8.775 5.25 5.25 0 0110.233-2.33 3 3 0 013.758 3.848A3.752 3.752 0 0118 19.5H6.75z" /></svg></div><div class="content"><h3>MQTT</h3><p><span class="status-dot )rawliteral";
  html += (mqttClient.connected() ? "connected" : "disconnected");
  html += R"rawliteral("></span>)rawliteral";
  html += (mqttConnected.load() ? "Connected" : "Disconnected");
  html += R"rawliteral(</p></div><div class="chevron"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M8.25 4.5l7.5 7.5-7.5 7.5" /></svg></div></a>
<a href="/rs485_settings" class="settings-item"><div class="icon"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M11.42 15.17L17.25 21A2.652 2.652 0 0021 17.25l-5.877-5.877M11.42 15.17l2.496-3.03c.528-1.036.92-2.166 1.056-3.346A4.502 4.502 0 0012 4.5c-2.485 0-4.5 2.015-4.5 4.5c0 1.18.392 2.31.956 3.234l2.496 3.03z" /></svg></div><div class="content"><h3>Modbus RTU (RS485) Sensor</h3><p><span class="status-dot connected"></span>Ready</p></div><div class="chevron"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M8.25 4.5l7.5 7.5-7.5 7.5" /></svg></div></a>
<a href="/modbustcp_settings" class="settings-item"><div class="icon"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M11.42 15.17L17.25 21A2.652 2.652 0 0021 17.25l-5.877-5.877M11.42 15.17l2.496-3.03c.528-1.036.92-2.166 1.056-3.346A4.502 4.502 0 0012 4.5c-2.485 0-4.5 2.015-4.5 4.5c0 1.18.392 2.31.956 3.234l2.496 3.03z" /></svg></div><div class="content"><h3>Modbus TCP Server</h3><p><span class="status-dot )rawliteral";
  html += (modbusTcpEnabled ? "connected" : "disconnected");
  html += R"rawliteral("></span>)rawliteral";
  html += (modbusTcpEnabled ? "Enabled" : "Disabled");
  html += R"rawliteral(</p></div><div class="chevron"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M8.25 4.5l7.5 7.5-7.5 7.5" /></svg></div></a>
<a href="/sensor_fault_settings" class="settings-item">Sensor communication alarm and gateway identity</a>
<a href="/relay_settings" class="settings-item"><div class="icon"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M12 9.75v6.75m0-6.75L9.75 12l2.25-2.25m0 6.75L14.25 12l-2.25 2.25M3.75 7.5h16.5M3.75 12h16.5m-16.5 4.5h16.5" /></svg></div><div class="content"><h3>Relay Outputs</h3><p><span class="status-dot )rawliteral";
  html += (relayAlertEnabled || relayAlarmEnabled ? "connected" : "disconnected");
  html += R"rawliteral("></span>)rawliteral";
  html += (relayAlertEnabled || relayAlarmEnabled ? "Enabled" : "Disabled");
  html += R"rawliteral(</p></div><div class="chevron"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M8.25 4.5l7.5 7.5-7.5 7.5" /></svg></div></a>
<a href="/device_info" class="settings-item"><div class="icon" style="color:#6c757d"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M11.25 11.25l.041-.02a.75.75 0 011.063.852l-.708 2.836a.75.75 0 001.063.852l.041-.021M21 12a9 9 0 11-18 0 9 9 0 0118 0z" /></svg></div><div class="content"><h3>Device Info</h3><p>View model, serial, and firmware</p></div><div class="chevron"><svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" d="M8.25 4.5l7.5 7.5-7.5 7.5" /></svg></div></a>
</div><div class="actions"><button class="btn btn-danger" onclick="rebootDevice()">Reboot Device</button><a href="/" class="btn btn-secondary">Back to Dashboard</a></div></div><script>function rebootDevice(){if(confirm("Are you sure you want to reboot the device?")){fetch("/reboot").then(()=>{alert("Device is rebooting...")})}}</script></body></html>)rawliteral";
  #if GATEWAY_ETHERNET
  html.replace("<h3>Wi-Fi</h3>", "<h3>Ethernet</h3>");
  html.replace("<div class=\"settings-list\">", "<div class=\"settings-list\"><a href=\"/ioc\" class=\"settings-item\"><div class=\"content\"><h3>IOC I/O &amp; Alarms</h3><p>Push-buttons, 4?20 mA inputs, alarm beacon and sounder</p></div></a>");
  #endif
  server.send(200, "text/html", html);
}

// ... All other handlers are the same ...
// ... from handleWifiSettings() to handleDeviceInfo() ...
void handleModbusTcpSettings() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Modbus TCP Settings</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;padding:20px 0;}
.container{width:100%;max-width:600px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1);}
.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}
.form-group{margin-bottom:20px;text-align:left}.form-group.inline{display:flex;align-items:center;gap:15px}
label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}
input[type=number]{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px}
.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600}
.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none}
table{width:100%;border-collapse:collapse;margin-top:20px}th,td{text-align:left;padding:10px;border-bottom:1px solid #dee2e6}th{background-color:#f8f9fa}
.ip-info{font-size:14px;color:#6c757d;text-align:center;background-color:#f8f9fa;padding:10px;border-radius:8px;margin-bottom:20px}
/* Toggle Switch CSS */
.switch{position:relative;display:inline-block;width:50px;height:28px}.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.4s;border-radius:28px}
.slider:before{position:absolute;content:"";height:20px;width:20px;left:4px;bottom:4px;background-color:white;transition:.4s;border-radius:50%}
input:checked+.slider{background-color:#28a745}
input:checked+.slider:before{transform:translateX(22px)}
</style></head><body><div class="container"><div class="header"><h2>Modbus TCP Server Settings</h2></div>
<div class="ip-info">Device IP Address: <strong>)rawliteral" + (gw::netConnected() ? gw::netIP().toString() : "N/A (AP Mode)") + R"rawliteral(</strong></div>
<form onsubmit="return saveSettings()">
<div class="form-group inline"><label for="enabled">Enable Modbus TCP Server</label><label class="switch"><input type="checkbox" id="enabled" name="enabled"><span class="slider"></span></label></div>
<div class="form-group"><label for="port">Server Port</label><input type="number" id="port" name="port" min="1" max="65535" required></div>
<button type="submit" class="btn-primary">Save and Reboot</button>
</form>
<div class="register-map">
<h3 style="margin-top:30px">Register Address Map (Read-Only)</h3>
<p>Values are scaled to integers. To get the real value, divide by the scaling factor.</p>
<table><thead><tr><th>Address</th><th>Metric Name</th><th>Unit</th><th>Scaling Factor</th></tr></thead><tbody>
)rawliteral";

    // Dynamically generate the table rows from our register map
    for (int i = 0; i < numRegistersInMap; i++) {
        html += "<tr>";
        html += "<td>" + String(i) + "</td>";
        html += "<td>" + String(registerMap[i].name) + "</td>";
        html += "<td>" + String(registerMap[i].unit) + "</td>";
        html += "<td>&divide; " + String(registerMap[i].scale, 0) + "</td>";
        html += "</tr>";
    }

    html += R"rawliteral(
</tbody></table></div>
<a href="/settings" class="nav-link">&larr; Back to Settings</a></div><script>
function saveSettings(){
    const isEnabled = document.getElementById('enabled').checked;
    const port = document.getElementById('port').value;
    alert('Settings saved. The device will now reboot to apply changes.');
    window.location.href = `/save_modbustcp?enabled=${isEnabled}&port=${port}`;
    return false;
}
window.onload=function(){
    document.getElementById('enabled').checked = )rawliteral" + String(modbusTcpEnabled ? "true" : "false") + R"rawliteral(;
    document.getElementById('port').value = )rawliteral" + String(modbusTcpPort) + R"rawliteral(;
};
</script></body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void handleSaveModbusTcp() {
    long validatedPort;
    if (!parseInteger(server.arg("port"), 1, 65535, validatedPort) || validatedPort == 80) {
        server.send(400, "text/plain", "Invalid TCP port (port 80 is reserved for the dashboard)."); return;
    }
    GatewayPreferences settings;
    if (!settings.begin("modbustcp-cfg", false)) {
        server.send(503, "text/plain", "Settings storage unavailable; no changes saved."); return;
    }
    settings.putBool("enabled", server.arg("enabled") == "true");
    settings.putUShort("port", static_cast<uint16_t>(validatedPort));
    const bool saved = settings.commit();
    settings.end();
    if (!saved) { server.send(503, "text/plain", "Settings could not be saved; device was not rebooted."); return; }

    server.send(200, "text/plain", "Settings saved. Rebooting now...");
    delay(1000);
    gw::restart();
}

// =================================================================
// --- SECTION: WEB SERVER HANDLERS (Continued)
// =================================================================

void handleWifiSettings() {
#if GATEWAY_ETHERNET
    server.send_P(200, "text/html; charset=utf-8", NETWORK_PAGE);
#else
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Wi-Fi Settings</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh}.container{width:100%;max-width:500px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1)}.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.form-group{margin-bottom:15px}label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}input[type=text],input[type=password]{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px;transition:border-color .3s}input[type=text]:focus,input[type=password]:focus{outline:0;border-color:#007bff}.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600;transition:background-color .3s}.btn-primary:hover{background-color:#0056b3}.btn-scan{background-color:#6c757d;margin-bottom:15px}.btn-scan:hover{background-color:#5a6268}.wifi-list-container{max-height:250px;overflow-y:auto;border:1px solid #e9ecef;border-radius:8px;padding:5px;background-color:#f8f9fa}#wifi-list{list-style:none;padding:0;margin:0}#wifi-list li{padding:12px 15px;cursor:pointer;border-bottom:1px solid #e9ecef;transition:background-color .2s;display:flex;justify-content:space-between;align-items:center}#wifi-list li:last-child{border-bottom:none}#wifi-list li:hover{background-color:#e9ecef}.wifi-ssid{font-size:16px;font-weight:500;flex-grow:1}.wifi-rssi{font-size:14px;color:#6c757d;font-weight:400}.status-bar{text-align:center;margin-top:20px;padding-top:20px;border-top:2px solid #e9ecef;font-size:14px;color:#6c757d;font-weight:500}.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;font-weight:600;font-size:14px}.nav-link:hover{text-decoration:underline}</style></head><body><div class="container"><div class="header"><h2>Wi-Fi Settings</h2></div><div class="status-bar">Current Status: )rawliteral" + htmlEscape(gw::netDescription()) + R"rawliteral(<br>IP Address: )rawliteral" + (gw::netConnected() ? gw::netIP().toString() : gw::netIP().toString()) + R"rawliteral(</div><div class="form-section"><button class="btn-primary btn-scan" onclick="scanWifi()">Scan for Networks</button><div class="wifi-list-container"><ul id="wifi-list"></ul></div><form onsubmit="return saveWifi()"><div class="form-group"><label for="ssid">Network Name (SSID)</label><input type="text" id="ssid" name="ssid" required></div><div class="form-group"><label for="pass">Password</label><input type="password" id="pass" name="pass"></div><button type="submit" class="btn-primary">Save and Connect</button></form></div><a href="/settings" class="nav-link">&larr; Back to Settings</a></div><script>function scanWifi(){const t=document.getElementById("wifi-list");t.innerHTML="<li>Scanning...</li>",fetch("/scanwifi").then(t=>t.json()).then(e=>{t.innerHTML="",0===e.length?t.innerHTML="<li>No networks found.</li>":e.forEach(e=>{const n=document.createElement("li");n.textContent=`${e.ssid} (${e.rssi} dBm)`,n.onclick=()=>{document.getElementById("ssid").value=e.ssid},t.appendChild(n)})})}function saveWifi(){const t=document.getElementById("ssid").value,e=document.getElementById("pass").value;fetch("/savewifi",{method:"POST",body:new URLSearchParams({ssid:t,pass:e})}).then(async r=>{const text=await r.text();if(!r.ok)throw Error(text);alert(text)}).catch(e=>alert("Save failed: "+e.message));return false}</script></body></html>)rawliteral";
    server.send(200, "text/html", html);
#endif
}

void handleSaveWifi() {
#if GATEWAY_ETHERNET
    server.send(400, "application/json", "{\"error\":\"IRIV uses Ethernet; Wi-Fi is unavailable\"}");
#else
    if (server.arg("ssid").isEmpty() || server.arg("ssid").length() > 32 || server.arg("pass").length() > 64) {
        server.send(400, "text/plain", "Invalid SSID or password length."); return;
    }
  GatewayPreferences settings;
  if (!settings.begin("wifi-creds", false)) {
      server.send(503, "text/plain", "Settings storage unavailable; no changes saved."); return;
  }
  settings.putString("ssid", server.arg("ssid"));
  settings.putString("password", server.arg("pass"));
  const bool saved = settings.commit();
  settings.end();
  if (!saved) { server.send(503, "text/plain", "Credentials could not be saved; device was not rebooted."); return; }
  server.send(200, "text/plain", "Credentials saved. Device will reboot.");
  delay(1000);
  gw::restart();
#endif
}

void handleMqttSettings() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>MQTT Settings</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh}.container{width:100%;max-width:500px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1)}.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.form-group{margin-bottom:15px}label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}input[type=text],input[type=password],input[type=number]{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px;transition:border-color .3s}input[type=text]:focus,input[type=password]:focus,input[type=number]:focus{outline:0;border-color:#007bff}.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600;transition:background-color .3s}.btn-primary:hover{background-color:#0056b3}.status-bar{text-align:center;margin-top:20px;padding-top:20px;border-top:2px solid #e9ecef;font-size:14px;color:#6c757d;font-weight:500}.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;font-weight:600;font-size:14px}.nav-link:hover{text-decoration:underline}</style></head><body><div class="container"><div class="header"><h2>MQTT Settings</h2></div><p>TLS requires the broker root CA configured in firmware. Port 1883 uses plain MQTT.</p><form onsubmit="return saveMqtt()"><div class="form-group"><label for="server">Broker Address</label><input type="text" id="server" name="server" value=")rawliteral" + htmlEscape(mqtt_server_str) + R"rawliteral(" required></div><div class="form-group"><label for="port">Port</label><input type="number" id="port" name="port" value=")rawliteral" + String(mqtt_port_val) + R"rawliteral(" required></div><div class="form-group"><label for="user">Username</label><input type="text" id="user" name="user" value=")rawliteral" + htmlEscape(mqtt_user_str) + R"rawliteral("></div><div class="form-group"><label for="pass">Password (leave blank to keep saved password)</label><input type="password" id="pass" name="pass" autocomplete="new-password" value=")rawliteral" + String("") + R"rawliteral("></div><div class="form-group"><label for="topic">Topic</label><input type="text" id="topic" name="topic" value=")rawliteral" + htmlEscape(mqtt_topic_str) + R"rawliteral(" required></div><button type="submit" class="btn-primary">Save and Reboot</button></form><div class="status-bar" id="status-bar">Current settings are displayed above. Saving will reboot the device.</div><a href="/settings" class="nav-link">&larr; Back to Settings</a></div><script>function saveMqtt(){const t=document.getElementById("server").value,e=document.getElementById("port").value,n=document.getElementById("user").value,o=document.getElementById("pass").value,r=document.getElementById("topic").value,s=document.getElementById("status-bar");return s.innerHTML="Saving MQTT settings...",fetch("/save_mqtt",{method:"POST",body:new URLSearchParams({server:t,port:e,user:n,pass:o,topic:r})}).then(response=>{if(!response.ok)throw new Error("Save failed; check settings");return response.text()}).then(text=>{s.textContent=text}).catch(error=>{s.textContent=error.message}),!1}</script></body></html>)rawliteral";
#if GATEWAY_ETHERNET
    html.replace("TLS requires the broker root CA configured in firmware. Port 1883 uses plain MQTT.", "IRIV supports plain MQTT on port 1883 only.");
#endif
    server.send(200, "text/html", html);
}

void handleSaveMqtt() {
    long port;
    const String topic = server.arg("topic");
    if (!parseInteger(server.arg("port"), 1, 65535, port) ||
        server.arg("server").isEmpty() || server.arg("server").length() > 253 ||
        server.arg("user").length() > 128 || server.arg("pass").length() > 128 ||
        topic.isEmpty() || topic.length() > 128 || topic.indexOf('#') >= 0 || topic.indexOf('+') >= 0) {
        server.send(400, "text/plain", "Invalid broker, port or publish topic."); return;
    }
#if GATEWAY_ETHERNET
    if (port != 1883) { server.send(400, "text/plain", "IRIV supports plain MQTT on port 1883 only."); return; }
#endif
    GatewayPreferences settings;
    if (!settings.begin("mqtt-cfg", false)) {
        server.send(503, "text/plain", "Settings storage unavailable; no changes saved."); return;
    }
    // Stage every field, including the old password when migrating legacy NVS.
    const String password = server.arg("pass").isEmpty()
        ? settings.getString("pass", mqtt_pass_str) : server.arg("pass");
    settings.putString("server", server.arg("server"));
    settings.putInt("port", static_cast<int32_t>(port));
    settings.putString("user", server.arg("user"));
    settings.putString("pass", password);
    settings.putString("topic", topic);
    const bool saved = settings.commit();
    settings.end();
    if (!saved) { server.send(503, "text/plain", "MQTT settings could not be saved; device was not rebooted."); return; }
    server.send(200, "text/plain", "MQTT credentials saved. Device will reboot.");
    delay(1000);
    gw::restart();
}

void handleRs485Settings() {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    const uint8_t displayedId = modbus_slave_id_val; const long displayedBaud = modbus_baud_rate_val;
    xSemaphoreGive(dataMutex);
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Modbus RS485 Settings</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh}.container{width:100%;max-width:500px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1)}.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.form-group{margin-bottom:15px}label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}input[type=number],select{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px;transition:border-color .3s}input[type=number]:focus,select:focus{outline:0;border-color:#007bff}.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600;transition:background-color .3s}.btn-primary:hover{background-color:#0056b3}.status-bar{text-align:center;margin-top:20px;padding-top:20px;border-top:2px solid #e9ecef;font-size:14px;color:#6c757d;font-weight:500}.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;font-weight:600;font-size:14px}.nav-link:hover{text-decoration:underline}</style></head><body><div class="container"><div class="header"><h2>Modbus RS485 Settings</h2><p><a href="/sensor_setup">Find sensor, change sensor settings and diagnostics</a></p></div><form onsubmit="return saveRs485()"><div class="form-group"><label for="slave_id">Slave ID</label><input type="number" id="slave_id" name="slave_id" min="1" max="247" value=")rawliteral" + String(displayedId) + R"rawliteral(" required></div><div class="form-group"><label for="baud_rate">Baud Rate</label><select id="baud_rate" name="baud_rate" required><option value="9600">9600</option><option value="19200">19200</option><option value="38400">38400</option><option value="57600">57600</option><option value="115200">115200</option></select></div><button type="submit" class="btn-primary">Save and Reboot</button></form><div class="status-bar" id="status-bar">Current settings are displayed above. Saving will reboot the device.</div><a href="/settings" class="nav-link">&larr; Back to Settings</a></div><script>function saveRs485(){const t=document.getElementById("slave_id").value,e=document.getElementById("baud_rate").value,n=document.getElementById("status-bar");n.textContent="Saving RS485 settings...";fetch(`/save_rs485?slave_id=${t}&baud_rate=${e}`).then(async r=>{const text=await r.text();if(!r.ok)throw Error(text);n.textContent=text}).catch(e=>{n.textContent="Save failed: "+e.message});return false}window.onload=function(){document.getElementById("baud_rate").value=")rawliteral" + String(displayedBaud) + R"rawliteral("};</script></body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void handleSaveRs485() {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    const bool setupBusy = sensorSetupBusy;
    if (!setupBusy) { sensorSetupBusy = true; sensorSetupMode = 0; }
    xSemaphoreGive(dataMutex);
    if (setupBusy) { server.send(409, "text/plain", "Sensor setup busy"); return; }
    struct ReleaseSetup { ~ReleaseSetup() { xSemaphoreTake(dataMutex, portMAX_DELAY); sensorSetupBusy=false; xSemaphoreGive(dataMutex); } } releaseSetup;
    long slave, baud;
    if (!parseInteger(server.arg("slave_id"), 1, 247, slave) ||
        !parseInteger(server.arg("baud_rate"), 9600, 115200, baud) ||
        (baud != 9600 && baud != 19200 && baud != 38400 && baud != 57600 && baud != 115200)) {
        server.send(400, "text/plain", "Invalid slave ID or baud rate."); return;
    }
    GatewayPreferences settings;
    if (!settings.begin("rs485-cfg", false)) {
        server.send(503, "text/plain", "Settings storage unavailable; no changes saved."); return;
    }
    settings.putUChar("slave_id", static_cast<uint8_t>(slave));
    settings.putLong("baud_rate", baud);
    const bool saved = settings.commit();
    settings.end();
    if (!saved) { server.send(503, "text/plain", "RS485 settings could not be saved; device was not rebooted."); return; }
    server.send(200, "text/plain", "RS485 settings saved. Device will reboot.");
    delay(1000);
    gw::restart();
}

void handleTriggerWaveform() {
  bool isBusy = true;
  if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    isBusy = isReadingWaveform || isReadingSpectrum ||
             currentDataState != DD_IDLE || dynamicStreamActive || sensorSetupBusy;
    if (!isBusy) {
      if (!sensorSetupBusy) currentDataState = DD_REQUEST_DATA;
    }
    xSemaphoreGive(dataMutex);
  }

  if (!isBusy) {
    server.send(200, "text/plain", "Trigger request accepted.");
  } else {
    server.send(409, "text/plain", "Busy, please wait for current operation to finish.");
  }
}

void handleSetIntervals() {
  if (server.hasArg("interval")) {
    long newInterval;
    if (!parseInteger(server.arg("interval"), 1000, INTERVAL_NEVER, newInterval)) {
        server.send(400, "text/plain", "Interval must be 1000..2147483647 ms."); return;
    }
    if (!saveCaptureInterval(newInterval)) {
        server.send(503, "text/plain", "Interval could not be saved; previous interval remains active."); return;
    }
    server.send(200, "text/plain", "Interval updated and saved.");
  } else {
    server.send(400, "text/plain", "Missing 'interval' argument.");
  }
}

void handleGetIntervals() {
    StaticJsonDocument<128> doc;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      doc["interval"] = dataCaptureInterval;
      xSemaphoreGive(dataMutex);
    }
    String payload;
    serializeJson(doc, payload);
    server.send(200, "application/json", payload);
}

void handleScanWifi() {
#if GATEWAY_ETHERNET
    server.send(400, "application/json", "{\"error\":\"IRIV uses Ethernet; Wi-Fi is unavailable\"}");
#else
    int n = WiFi.scanNetworks();
    if (n < 0) {
        WiFi.scanDelete();
        server.send(503, "application/json", "{\"error\":\"Wi-Fi scan failed\"}");
        return;
    }
    n = min(n, 20);
    DynamicJsonDocument doc(JSON_ARRAY_SIZE(n) + n * (JSON_OBJECT_SIZE(2) + 33) + 100);
    JsonArray networks = doc.to<JsonArray>();
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
      }
    }
    String json;
    serializeJson(doc, json);
    WiFi.scanDelete();
    server.send(200, "application/json", json);
#endif
}

void handleGetLogData() {
    DynamicJsonDocument doc(12288);

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        doc["is_recording"] = isPeakRecording;
        doc["log_count"] = peakLogCount;
        doc["is_baseline_established"] = isBaselineEstablished;
        doc["baseline_samples"] = baselineSamplesCollected;
        doc["max_log_size"] = PEAK_LOG_SIZE;
        doc["max_baseline_samples"] = BASELINE_SAMPLES_REQUIRED;
        doc["anomaly_detected"] = anomalyDetected;
        doc["anomaly_details"] = anomalyDetails;

        JsonArray b_freqs = doc.createNestedArray("baseline_freqs");
        if(isBaselineEstablished) {
            for(int i=0; i<NUM_PEAKS_TO_TRACK; ++i) {
                if(baselines[i].is_set) { b_freqs.add(baselines[i].frequency); }
            }
        }
        JsonArray i_freqs = doc.createNestedArray("initial_freqs");
        if(peakLogCount > 0) {
            int first_valid_index = (peakLogIndex - peakLogCount + PEAK_LOG_SIZE) % PEAK_LOG_SIZE;
             if(peakLog[first_valid_index].is_valid){
                 for(int p=0; p<NUM_PEAKS_TO_TRACK; ++p) { i_freqs.add(peakLog[first_valid_index].peaks[p].frequency); }
             }
        }

        JsonArray log = doc.createNestedArray("log");
        for (int i = 0; i < peakLogCount; ++i) {
            int currentIndex = (peakLogIndex - peakLogCount + i + PEAK_LOG_SIZE) % PEAK_LOG_SIZE;
            if(peakLog[currentIndex].is_valid) {
                JsonObject entry = log.createNestedObject();
                entry["timestamp"] = String(peakLog[currentIndex].timestamp);
                JsonArray peaks = entry.createNestedArray("peaks");
                for(int p=0; p<NUM_PEAKS_TO_TRACK; ++p) {
                    JsonObject peak = peaks.createNestedObject();
                    peak["f"] = peakLog[currentIndex].peaks[p].frequency;
                    peak["a"] = peakLog[currentIndex].peaks[p].amplitude;
                }
            }
        }
        xSemaphoreGive(dataMutex);
    }
    String payload;
    serializeJson(doc, payload);
    server.send(200, "application/json", payload);
}

void handleDataLogControl() {
    const String action = server.arg("action");
    if (action != "start" && action != "stop") { server.send(400, "text/plain", "Action must be start or stop."); return; }
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { server.send(503, "text/plain", "System busy."); return; }
    isPeakRecording = action == "start";
    ++recordingGeneration;
    anomalyDetected = false;
    anomalyDetails = "";
    if (isPeakRecording) {
        peakLogIndex = 0; peakLogCount = 0; baselineSamplesCollected = 0;
        isBaselineEstablished = false;
        for (auto& entry : peakLog) entry.is_valid = false;
        for (auto& baseline : baselines) baseline.is_set = false;
    }
    snprintf(systemStatus, sizeof(systemStatus), "%s", isPeakRecording ? "Recording started (Learning)" : "Recording stopped");
    xSemaphoreGive(dataMutex);
    server.send(200, "text/plain", action == "start" ? "Recording started." : "Recording stopped.");
}

void handleDownloadCsv() {
    SdLock sd;
    if (!sd) { server.send(503, "text/plain", "Storage busy; retry shortly."); return; }
    if (currentStorageMode == STORAGE_SD) {
        File file = SD.open("/peak_log.csv");
        if(!file || file.size() == 0){
            server.send(404, "text/plain", "File not found or empty");
            return;
        }
        server.sendHeader("Content-Disposition", "attachment; filename=\"peak_log_sd.csv\"");
        server.streamFile(file, "text/csv");
        file.close();
    } else if (currentStorageMode == STORAGE_INTERNAL) {
        DataSnapshot rows[PEAK_LOG_SIZE];
        int count, index;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) != pdTRUE) { server.send(503, "text/plain", "System busy."); return; }
        memcpy(rows, peakLog, sizeof(rows));
        count = peakLogCount; index = peakLogIndex;
        xSemaphoreGive(dataMutex);

        server.sendHeader("Content-Type", "text/csv");
        server.sendHeader("Content-Disposition", "attachment; filename=\"peak_log_internal.csv\"");
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200);

        String header = "Timestamp,Peak 1 Freq (Hz),Peak 1 Amp (g),Peak 2 Freq (Hz),Peak 2 Amp (g),Peak 3 Freq (Hz),Peak 3 Amp (g),Peak 4 Freq (Hz),Peak 4 Amp (g)\n";
        server.sendContent(header);


            for (int i = 0; i < count; ++i) {
                int currentIndex = (index - count + i + PEAK_LOG_SIZE) % PEAK_LOG_SIZE;
                if(rows[currentIndex].is_valid) {
                    String row = String(rows[currentIndex].timestamp);
                    for(int p=0; p<NUM_PEAKS_TO_TRACK; ++p) {
                        row += "," + String(rows[currentIndex].peaks[p].frequency, 2);
                        row += "," + String(rows[currentIndex].peaks[p].amplitude, 4);
                    }
                    row += "\n";
                    server.sendContent(row);
                }
            }

        server.sendContent("");
    } else {
        server.send(404, "text/plain", "No storage available or no data logged.");
    }
}

void handleGetMetricsLogData() {
    DynamicJsonDocument doc(12288);
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        doc["is_recording"] = isMetricsRecording;
        doc["log_count"] = metricsLogCount;
        doc["max_log_size"] = METRICS_LOG_SIZE;
        JsonArray log = doc.createNestedArray("log");
        for (int i = 0; i < metricsLogCount; ++i) {
            int currentIndex = (metricsLogIndex - metricsLogCount + i + METRICS_LOG_SIZE) % METRICS_LOG_SIZE;
            if(metricsLog[currentIndex].is_valid) {
                JsonObject entry = log.createNestedObject();
                entry["timestamp"] = String(metricsLog[currentIndex].timestamp);
                JsonArray vals = entry.createNestedArray("values");
                for(int v=0; v<14; ++v) {
                    vals.add(metricsLog[currentIndex].values[v]);
                }
            }
        }
        xSemaphoreGive(dataMutex);
    }
    String payload;
    serializeJson(doc, payload);
    server.send(200, "application/json", payload);
}

void handleMetricsLogControl() {
    const String action = server.arg("action");
    if (action != "start" && action != "stop") { server.send(400, "text/plain", "Action must be start or stop."); return; }
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { server.send(503, "text/plain", "System busy."); return; }
    isMetricsRecording = action == "start";
    if (isMetricsRecording) {
        metricsLogIndex = 0; metricsLogCount = 0;
        for (auto& entry : metricsLog) entry.is_valid = false;
    }
    xSemaphoreGive(dataMutex);
    server.send(200, "text/plain", action == "start" ? "Metrics recording started." : "Metrics recording stopped.");
}

void handleDownloadMetricsCsv() {
    SdLock sd;
    if (!sd) { server.send(503, "text/plain", "Storage busy; retry shortly."); return; }
    if (currentStorageMode == STORAGE_SD) {
        File file = SD.open("/metrics_log.csv");
        if(!file || file.size() == 0){
            server.send(404, "text/plain", "File not found or empty");
            return;
        }
        server.sendHeader("Content-Disposition", "attachment; filename=\"metrics_log_sd.csv\"");
        server.streamFile(file, "text/csv");
        file.close();
    } else if (currentStorageMode == STORAGE_INTERNAL) {
        MetricsSnapshot rows[METRICS_LOG_SIZE];
        int count, index;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) != pdTRUE) { server.send(503, "text/plain", "System busy."); return; }
        memcpy(rows, metricsLog, sizeof(rows));
        count = metricsLogCount; index = metricsLogIndex;
        xSemaphoreGive(dataMutex);

        server.sendHeader("Content-Type", "text/csv");
        server.sendHeader("Content-Disposition", "attachment; filename=\"metrics_log_internal.csv\"");
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200);

        String header = "Timestamp,Accel RMS 1 (g),Accel Peak 1 (g),Accel RMS 2 (g),Accel Peak 2 (g),Velo RMS (mm/s),Velo Peak (mm/s),Displ RMS (um),Displ Peak (um),True Peak (g),Crest Factor,Std. Deviation,Temperature (C),Drive Voltage (V),Board Temp (C)\n";
        server.sendContent(header);


            for (int i = 0; i < count; ++i) {
                int currentIndex = (index - count + i + METRICS_LOG_SIZE) % METRICS_LOG_SIZE;
                if(rows[currentIndex].is_valid) {
                    String row = String(rows[currentIndex].timestamp);
                    for(int v=0; v<14; ++v) {
                        row += "," + String(rows[currentIndex].values[v], 3);
                    }
                    row += "\n";
                    server.sendContent(row);
                }
            }

        server.sendContent("");
    } else {
        server.send(404, "text/plain", "No storage available or no data logged.");
    }
}

void handleClearPeakLog() {
    SdLock sd;
    if (!sd) { server.send(503, "text/plain", "Storage busy; retry shortly."); return; }
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      ++recordingGeneration;
      peakLogIndex = 0; peakLogCount = 0; baselineSamplesCollected = 0;
      isBaselineEstablished = false; anomalyDetected = false;
      for(int i=0; i<PEAK_LOG_SIZE; ++i) peakLog[i].is_valid = false;
      for(int i=0; i<NUM_PEAKS_TO_TRACK; ++i) baselines[i].is_set = false;
      if(isPeakRecording) { snprintf(systemStatus, sizeof(systemStatus), "Recording started (Learning)"); }
      xSemaphoreGive(dataMutex);
    } else { server.send(503, "text/plain", "System busy; data not cleared."); return; }
    if (currentStorageMode == STORAGE_SD) {
        SD.remove("/peak_log.csv");
    }
    server.send(200, "text/plain", "Peak data log cleared.");
}

void handleClearMetricsLog() {
    SdLock sd;
    if (!sd) { server.send(503, "text/plain", "Storage busy; retry shortly."); return; }
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      metricsLogIndex = 0; metricsLogCount = 0;
      for(int i=0; i<METRICS_LOG_SIZE; ++i) metricsLog[i].is_valid = false;
      xSemaphoreGive(dataMutex);
    } else { server.send(503, "text/plain", "System busy; data not cleared."); return; }
    if (currentStorageMode == STORAGE_SD) {
        SD.remove("/metrics_log.csv");
    }
    server.send(200, "text/plain", "Metrics data log cleared.");
}

void handleClearDynamicData() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      newWaveformAvailable = false;
      newSpectrumAvailable = false;
      xSemaphoreGive(dataMutex);
    } else { server.send(503, "text/plain", "System busy; data not cleared."); return; }
    server.send(200, "text/plain", "Cached waveform and spectrum data cleared.");
}

void handleGetEventLog() {
    SdLock sd;
    if (!sd) { server.send(503, "text/plain", "Storage busy; retry shortly."); return; }
    DynamicJsonDocument doc(2048);
    JsonArray files = doc.createNestedArray("files");

    if (currentStorageMode == STORAGE_SD) {
        File root = SD.open("/");
        if (root) {
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    JsonObject fileObj = files.createNestedObject();
                    fileObj["name"] = String(file.name());
                    fileObj["size"] = file.size();

                    time_t lw = file.getLastWrite();
                    struct tm tmstruct;
                    localtime_r(&lw, &tmstruct);
                    char time_buf[22];
                    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tmstruct);
                    fileObj["timestamp"] = String(time_buf);
                }
                file.close();
                file = root.openNextFile();
            }
            root.close();
        }
    }

    String payload;
    serializeJson(doc, payload);
    server.send(200, "application/json", payload);
}

void handleClearEventLog() {
    SdLock sd;
    if (!sd) { server.send(503, "text/plain", "Storage busy; retry shortly."); return; }
    if (currentStorageMode == STORAGE_SD) {
        SD.remove("/event_log.csv");
    }
    server.send(200, "text/plain", "event_log.csv cleared.");
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting device now...");
  delay(1000);
  gw::restart();
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void handleRelaySettings() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Relay Settings</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh}.container{width:100%;max-width:600px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1)}.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.form-group{margin-bottom:15px;text-align:left}.form-group.inline{display:flex;align-items:center;gap:10px}label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}input[type=text],input[type=number]{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px}input[type=checkbox]{width:20px;height:20px}.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600;transition:background-color .3s}.btn-primary:hover{background-color:#0056b3}.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none}.grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,220px),1fr));gap:20px}.section{border:1px solid #e9ecef;border-radius:8px;padding:20px}h3{margin-top:0;border-bottom:1px solid #e9ecef;padding-bottom:10px;margin-bottom:15px}</style></head><body><div class="container"><div class="header"><h2>Relay Output Settings</h2></div><form onsubmit="return saveRelay()"><div class="grid-2"><div class="section"><h3>Alert Relay</h3><div class="form-group inline"><input type="checkbox" id="alert_en" name="alert_en"><label for="alert_en">Enable Alert Relay</label></div><div class="form-group"><label for="alert_thr">Alert Threshold (Velo Peak mm/s)</label><input type="number" step="0.1" id="alert_thr" name="alert_thr" required></div></div><div class="section"><h3>Alarm Relay</h3><div class="form-group inline"><input type="checkbox" id="alarm_en" name="alarm_en"><label for="alarm_en">Enable Alarm Relay</label></div><div class="form-group"><label for="alarm_thr">Alarm Threshold (Velo Peak mm/s)</label><input type="number" step="0.1" id="alarm_thr" name="alarm_thr" required></div></div></div><div class="section" style="margin-top:20px"><h3>Common Settings</h3><div class="form-group"><label for="delay">Trigger Delay (milliseconds)</label><input type="number" id="delay" name="delay" required></div></div><button type="submit" class="btn-primary" style="margin-top:20px">Save and Apply</button></form><a href="/settings" class="nav-link">&larr; Back to Settings</a></div><script>
    function saveRelay(){
        const params = new URLSearchParams({
            alert_en: document.getElementById('alert_en').checked,
            alert_thr: document.getElementById('alert_thr').value,
            alarm_en: document.getElementById('alarm_en').checked,
            alarm_thr: document.getElementById('alarm_thr').value,
            delay: document.getElementById('delay').value
        });

        fetch(`/save_relay?${params.toString()}`)
            .then(response => response.text())
            .then(text => alert(text));

        return false;
    }

    window.onload = function() {
        document.getElementById('alert_en').checked = )rawliteral" + String(relayAlertEnabled ? "true" : "false") + R"rawliteral(;
        document.getElementById('alarm_en').checked = )rawliteral" + String(relayAlarmEnabled ? "true" : "false") + R"rawliteral(;
        document.getElementById('alert_thr').value = )rawliteral" + String(veloPeakAlertThreshold) + R"rawliteral(;
        document.getElementById('alarm_thr').value = )rawliteral" + String(veloPeakAlarmThreshold) + R"rawliteral(;
        document.getElementById('delay').value = )rawliteral" + String(relayTriggerDelay) + R"rawliteral(;
    };
</script></body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void handleSaveRelay() {
    float alert, alarm;
    long delayMs;
    if (!parseThreshold(server.arg("alert_thr"), alert) ||
        !parseThreshold(server.arg("alarm_thr"), alarm) || alarm < alert ||
        !parseInteger(server.arg("delay"), 0, 3600000, delayMs)) {
        server.send(400, "text/plain", "Invalid thresholds or delay; alarm must be at least alert."); return;
    }
    const bool alertEnabled = server.arg("alert_en") == "true";
    const bool alarmEnabled = server.arg("alarm_en") == "true";
    GatewayPreferences settings;
    if (!settings.begin("relay-cfg", false)) {
        server.send(503, "text/plain", "Settings storage unavailable; relay settings unchanged."); return;
    }
    settings.putBool("alert_en", alertEnabled);
    settings.putBool("alarm_en", alarmEnabled);
    settings.putFloat("alert_thr", alert);
    settings.putFloat("alarm_thr", alarm);
    settings.putULong("delay", delayMs);
    const bool saved = settings.commit();
    settings.end();
    if (!saved) { server.send(503, "text/plain", "Relay settings could not be saved; previous settings remain active."); return; }
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    relayAlertEnabled = alertEnabled;
    relayAlarmEnabled = alarmEnabled;
    veloPeakAlertThreshold = alert;
    veloPeakAlarmThreshold = alarm;
    relayTriggerDelay = delayMs;
    alertThresholdExceededTime = 0;
    alarmThresholdExceededTime = 0;
    xSemaphoreGive(dataMutex);
    server.send(200, "text/plain", "Relay settings saved.");
}

void handleDeviceInfo() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Device Info</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh}.container{width:100%;max-width:500px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1)}.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.info-list{list-style:none;padding:0;margin:0}.info-item{display:flex;justify-content:space-between;padding:15px 10px;border-bottom:1px solid #e9ecef}.info-item:last-child{border-bottom:none}.info-label{font-weight:600;color:#343a40}.info-value{color:#6c757d}.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;font-weight:600;font-size:14px}.nav-link:hover{text-decoration:underline}</style></head><body><div class="container"><div class="header"><h2>Device Info</h2></div><div class="info-list">
<div class="info-item"><span class="info-label">Model</span><span class="info-value">)rawliteral" + String(DEVICE_MODEL) + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Sensor serial (unverified)</span><span class="info-value">)rawliteral" + String(DEVICE_SERIAL_NUMBER) + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Gateway ID (MQTT target)</span><span class="info-value">)rawliteral" + gateway_id + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Firmware Version</span><span class="info-value">)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Uptime</span><span class="info-value">)rawliteral" + formatUptime(millis()) + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Free RAM</span><span class="info-value">)rawliteral" + String(gw::freeHeap() / 1024.0, 2) + R"rawliteral( KB</span></div>
</div><a href="/settings" class="nav-link">&larr; Back to Settings</a></div></body></html>
)rawliteral";
    server.send(200, "text/html", html);
}
