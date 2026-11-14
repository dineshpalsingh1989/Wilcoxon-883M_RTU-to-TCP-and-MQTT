#pragma once
#include "core/globals.h"

// Error codes reported to monitoring and the dashboard ("Modbus error 0x..").
// They keep the values ModbusMaster used; 1-11 are sensor exception codes.
const uint8_t RTU_ERROR_INVALID_RESPONSE = 0xE0;
const uint8_t RTU_ERROR_TIMEOUT = 0xE2;
const uint8_t RTU_ERROR_CRC = 0xE3;
const uint8_t RTU_ERROR_GATEWAY = 0xE4; // UART not ready, write failure or invalid request.

void beginSensorRtu();
void fetchSensorIdentity();
bool requestNewWaveform();
bool isWaveformReady();
// Reads sensor input registers; returns 0 or one of the error codes above.
uint8_t readSensorRegisters(uint16_t address, uint16_t count, uint16_t* values);
bool readSensorValue(const char* name, uint16_t address, float scale, float &target);
bool readLargeDataBlock(uint16_t startAddress, uint16_t totalPoints, int16_t* buffer);
