#pragma once
#include <stdint.h>

#if defined(BOARD_CYTRON_IRIV_IOC)
// Cytron IRIV IO Controller (RP2350A): pins from its datasheet. Isolated RS485 on UART1 with
// automatic direction control, so there is no DE/RE pin. Network is wired Ethernet (W5500).
#define STATUS_LED_PIN 29  // USR LED
#define BUZZER_PIN 11      // Internal passive buzzer
#define USER_BUTTON_PIN 28 // USER button, active low
#define RXD_PIN        25  // UART1 RX (Serial2) to the isolated RS485 driver
#define TXD_PIN        24  // UART1 TX (Serial2)
#define RS485_CTRL_PIN -1  // Direction is switched by the RS485 driver itself
#define ETHERNET_HOSTNAME "ila-gateway-883m"
#define ETHERNET_FALLBACK_IP "192.168.4.1" // used when no DHCP server answers
#else
#if defined(BOARD_CYTRON_MAKER_ESP32)
// Cytron Maker ESP32: no RGB LED; the onboard GPIO2 LED shows status instead.
#define STATUS_LED_PIN 2
#define BUZZER_PIN 26     // Onboard piezo (has a mute switch)
#define USER_BUTTON_PIN 4 // Onboard user button
#else
#define RGB_PIN 48
#define NUM_PIXELS 1
#define BUZZER_PIN 5
#define USER_BUTTON_PIN 0
#endif
#define RXD_PIN        18 // Connect to RO (Receiver Output)
#define TXD_PIN        17 // Connect to DI (Driver Input)
#define RS485_CTRL_PIN 14 // Connect to DE and RE
#endif
#define WIFI_AP_SSID "ILA_Sensor_AP"
#define WIFI_AP_PASS "12345678"
#define HREG_DATA_TYPE_SELECT 80        
#define HREG_WAVEFORM_READY_STATUS 90
#define HREG_SPECTRUM_READY_STATUS 91
#define HREG_TRIGGER_WAVEFORM 92
#define HREG_TRIGGER_SPECTRUM 93
#define HREG_WAVEFORM_START 1000        // Registers 1000 - 14333
#define HREG_SPECTRUM_START 20000       // Registers 20000 - 26144
#define REG_TRIGGER             0x0200 
#define VAL_TRIGGER_START       0xFFFF 
#define VAL_STATUS_READY        0x0000 
#define VAL_STATUS_BUSY         0xEEEE 
#define ADDR_WAVE_X_START       0x1E01
#define ADDR_WAVE_Y_START       0x7101
#define ADDR_WAVE_Z_START       0xC401
#define ADDR_SPEC_X_START       0x0600
#define ADDR_SPEC_Y_START       0x5900
#define ADDR_SPEC_Z_START       0xAC00
const uint16_t WAVEFORM_TOTAL_POINTS = 13334;
const uint16_t SPECTRUM_TOTAL_POINTS = 6145;
const float SPECTRUM_SCALE_FACTOR = 0.001; 
const float SPECTRUM_FREQ_RESOLUTION = (13333.333 / 16384.0); 
const int METRICS_LOG_SIZE = 30;
const int METRICS_SNAPSHOT_SIZE = 31 + 12; 
const long INTERVAL_NEVER = 2147483647L;
const long dataPollInterval = 500;
const int maxPollAttempts = 20;
const long mqttReconnectInterval = 5000;
const long healthPublishInterval = 300000; // also sent once right after MQTT connects
