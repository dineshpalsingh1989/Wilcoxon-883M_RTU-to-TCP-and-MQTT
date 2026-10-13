#include "core/globals.h"

// Global Mutex & Flags
SemaphoreHandle_t dataMutex;
bool shouldReboot = false; // Safe reboot flag

// --- Hardware Config (ESP32-S3-DevKitC-1) ---
// Onboard RGB LED is typically GPIO 48 on DevKitC-1
Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// External Buzzer (Connect to Pin 5)

// User Button (Using BOOT Button = GPIO 0)
// Pressing BOOT pulls this pin LOW.

// --- MAX485 PIN CONFIGURATION (Moved to UART1) ---
// We cannot use 43/44 on DevKitC-1 as they are used by the USB-Serial Bridge.
// Using standard UART1 pins: TX=17, RX=18.

// --- WiFi AP Defaults ---

// --- Device Info (Dynamic) ---
String sensor_model = "Wilcoxon 883M";
String sensor_serial = "Unknown";
String sensor_vendor_url = "Unknown";
String sensor_part_number = "Unknown";
String sensor_fw_version = "Unknown";
const char* FIRMWARE_VERSION = "4.7.0 - S3-DevKitC-1";

// --- NTP & Time Config ---
const char* ntpServer = "pool.ntp.org";
long  gmtOffset_sec = 28800; // Default GMT+8
int   daylightOffset_sec = 0;

// --- MQTT Configuration ---
bool mqttEnabled = true;
bool mqtt_use_tls = true;
bool mqtt_skip_cert_validation = true;
bool mqtt_publish_progress = true;
String mqtt_server_str = ""; 
int mqtt_port_val = 8883;
String mqtt_user_str = "";
String mqtt_pass_str = "";
String mqtt_topic_str = "883M";
String mqtt_command_topic = "883M/command";
String mqtt_status_topic = "883M/status";
char mqttStatus[128] = "Waiting for config...";

// --- Wi-Fi & Web Server ---
WebServer server(80);
Preferences preferences;
WiFiClient espClient; 
WiFiClientSecure espClientSecure; 
PubSubClient* mqttClient = nullptr; 

// --- Modbus RTU (RS485) ---
uint8_t modbus_slave_id_val = 1;
long modbus_baud_rate_val = 115200;
ModbusMaster node;

// --- Modbus TCP Server ---
bool modbusTcpEnabled = true;
uint16_t modbusTcpPort = 502;
ModbusTCP modbusTCPServer;

// --- LINEAR MODBUS MAP ---


// --- Modbus Addresses (Sensor Side) ---


// --- Global State ---
char systemStatus[128] = "Initializing...";
char trafficLightStatus[20] = "IDLE"; 
String globalLogBuffer = ""; 
String lastResetReason = "Unknown";
DynamicDataState currentDataState = DD_IDLE;
CaptureMode requestedCapture = CAPTURE_NONE;
bool isReadingWaveform = false;
int waveformReadIndex = 0;
bool isReadingSpectrum = false;
int spectrumReadIndex = 0;
Axis requestedAxis = AXIS_NONE;
Axis lastCapturedAxis = AXIS_NONE;
char lastCaptureTimestamp[21] = {0};
char lastAutoCaptureTimestamp[21] = "Never";
int autoCaptureAxisIndex = 0;
bool isAutoCaptureCycleActive = false;

// --- Button/Alarm ---
unsigned long buttonPressStartTime = 0;
bool apModeTriggeredByButton = false;
bool wasWifiConnected = false;
bool wifiDisconnectAlarmActive = false;
unsigned long lastAlarmActionTime = 0;
bool isAlarmBeeping = false;

// --- Global Metrics ---
float accelRmsX=0.0, accelRmsY=0.0, accelRmsZ=0.0;
float accelPeakX=0.0, accelPeakY=0.0, accelPeakZ=0.0;
float accelRms2X=0.0, accelRms2Y=0.0, accelRms2Z=0.0;
float accelPeak2X=0.0, accelPeak2Y=0.0, accelPeak2Z=0.0;
float veloRmsX=0.0, veloRmsY=0.0, veloRmsZ=0.0;
float veloPeakX=0.0, veloPeakY=0.0, veloPeakZ=0.0;
float displRmsX=0.0, displRmsY=0.0, displRmsZ=0.0;
float displPeakX=0.0, displPeakY=0.0, displPeakZ=0.0;
float truePeakX=0.0, truePeakY=0.0, truePeakZ=0.0;
float crestFactorX=0.0, crestFactorY=0.0, crestFactorZ=0.0;
float stdDeviationX=0.0, stdDeviationY=0.0, stdDeviationZ=0.0;
float temperature=0.0;
// Extended Metrics
float accelMetricRmsX=0.0, accelMetricRmsY=0.0, accelMetricRmsZ=0.0;     
float accelMetricPeakX=0.0, accelMetricPeakY=0.0, accelMetricPeakZ=0.0; 
float accelMetricRms2X=0.0, accelMetricRms2Y=0.0, accelMetricRms2Z=0.0; 
float accelMetricPeak2X=0.0, accelMetricPeak2Y=0.0, accelMetricPeak2Z=0.0; 
float veloWideRmsX=0.0, veloWideRmsY=0.0, veloWideRmsZ=0.0;
float veloWidePeakX=0.0, veloWidePeakY=0.0, veloWidePeakZ=0.0;
float displLowRmsX=0.0, displLowRmsY=0.0, displLowRmsZ=0.0;
float displLowPeakX=0.0, displLowPeakY=0.0, displLowPeakZ=0.0;

// --- Buffers ---
int16_t fullWaveform[WAVEFORM_TOTAL_POINTS];
bool newWaveformAvailable = false;
int16_t spectrumData[SPECTRUM_TOTAL_POINTS];
bool newSpectrumAvailable = false;
int16_t globalScratchBuffer[WAVEFORM_TOTAL_POINTS]; 

// --- Spectrum Helpers ---
std::map<Axis, std::vector<SpectrumPeak>> xyzPeakDataCache;

// --- Metrics Logger ---
MetricsSnapshot metricsLog[METRICS_LOG_SIZE];
bool isMetricsRecording = false;
int metricsLogIndex = 0;
int metricsLogCount = 0;

// --- Timers ---
unsigned long lastMetricsReadTime = 0;
long metricsReadInterval_ms = 2000;
unsigned long lastCaptureActionTime = 0;
unsigned long lastAutoCaptureMillis = 0;
long dataCaptureInterval = 600000;
unsigned long ledBlinkTime = 0;
bool ledState = false;
unsigned long lastPollTime = 0;
int pollAttemptCounter = 0;
unsigned long lastMqttAttemptTime = 0;
unsigned long lastHealthPublishTime = 0;
