#pragma once
#include "core/globals.h"

String formatUptime(unsigned long ms);
String getTimestamp();
void logMessage(String msg);
String getResetReasonString();
std::vector<SpectrumPeak> findSpectrumPeaks(const int16_t* spectrum_buffer, int num_peaks);
