#include "core/modules.h"

String formatUptime(unsigned long ms) {
    unsigned long seconds = ms / 1000;
    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;
    return String(days) + "d " + String(hours) + "h " + String(minutes) + "m";
}

String getTimestamp() {
  // Never wait for NTP here: without it (AP mode) a wait would block while dataMutex is held.
  time_t now = time(nullptr);
  if (now < 1600000000) { return "No Time Sync"; }
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char timeString[21];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo); // local time (no 'Z': not UTC)
  return String(timeString);
}

void logMessage(String msg) {
    Serial.println(msg);
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        String ts = getTimestamp();
        if(ts == "No Time Sync") ts = String(millis()/1000) + "s";
        const char* taskName = pcTaskGetName(NULL);
        String newEntry = ts + " [" + String(taskName) + "] " + msg + "\n";
        globalLogBuffer += newEntry;
        if (globalLogBuffer.length() > 3000) {
            globalLogBuffer = globalLogBuffer.substring(globalLogBuffer.length() - 3000);
        }
        xSemaphoreGive(dataMutex);
    }
}

String getResetReasonString() { return gw::resetReason(); }

std::vector<SpectrumPeak> findSpectrumPeaks(const int16_t* spectrum_buffer, int num_peaks) {
    // Only local maxima count as peaks, otherwise the top N bins are just neighbours of one peak.
    std::vector<SpectrumPeak> all_points;
    for (int i = 1; i < SPECTRUM_TOTAL_POINTS - 1; i++) {
        int16_t prev = abs(spectrum_buffer[i - 1]), cur = abs(spectrum_buffer[i]), next = abs(spectrum_buffer[i + 1]);
        if (cur > prev && cur >= next) {
            all_points.push_back({ .frequency_hz = i * SPECTRUM_FREQ_RESOLUTION, .amplitude_g = cur * SPECTRUM_SCALE_FACTOR });
        }
    }
    std::sort(all_points.begin(), all_points.end(), [](const SpectrumPeak& a, const SpectrumPeak& b) { return a.amplitude_g > b.amplitude_g; });
    if ((int)all_points.size() > num_peaks) { all_points.resize(num_peaks); }
    std::sort(all_points.begin(), all_points.end(), [](const SpectrumPeak& a, const SpectrumPeak& b) { return a.frequency_hz < b.frequency_hz; });
    return all_points;
}

