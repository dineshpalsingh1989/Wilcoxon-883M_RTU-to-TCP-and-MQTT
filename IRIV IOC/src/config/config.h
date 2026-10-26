#pragma once
#include <stdint.h>

#include "config/board_pins.h" // board pins, shared with the ILA200 firmware
#define HREG_IO_FIRST 70                // IRIV I/O state, registers 70-79
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
// Failed metrics reads in a row before sensor polling stops until Reconnect is pressed.
const uint8_t SENSOR_READ_ATTEMPTS = 3;
