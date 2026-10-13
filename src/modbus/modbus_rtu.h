#pragma once
#include "core/globals.h"

void fetchSensorIdentity();
bool requestNewWaveform();
bool isWaveformReady();
bool readSensorValue(const char* name, uint16_t address, float scale, float &target);
bool readLargeDataBlock(uint16_t startAddress, uint16_t totalPoints, int16_t* buffer);
