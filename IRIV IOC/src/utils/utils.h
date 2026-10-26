#pragma once
#include "core/globals.h"

String formatUptime(unsigned long ms);
String getTimestamp();
void logMessage(String msg);
String getResetReasonString();
String htmlEscape(const String& text); // for values placed inside HTML attributes
std::vector<SpectrumPeak> findSpectrumPeaks(const int16_t* spectrum_buffer, int num_peaks);
