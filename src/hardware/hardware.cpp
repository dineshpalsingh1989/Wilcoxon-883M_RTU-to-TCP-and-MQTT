#include "core/modules.h"

void updateLedStatus() {
    unsigned long currentMillis = millis();
    bool local_isReading = false;
    bool local_isMetricsRecording = false;
    char local_status[128];

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        local_isReading = isReadingWaveform || isReadingSpectrum || currentDataState == DD_WAITING_FOR_DATA;
        local_isMetricsRecording = isMetricsRecording;
        snprintf(local_status, sizeof(local_status), "%s", systemStatus);
        xSemaphoreGive(dataMutex);
    }

    if (local_isReading) {
        if (currentMillis - ledBlinkTime > 500) {
            ledBlinkTime = currentMillis;
            ledState = !ledState;
            pixels.setPixelColor(0, ledState ? pixels.Color(255, 255, 0) : pixels.Color(0, 0, 0)); // Yellow
            pixels.show();
        }
        return;
    }
      if (local_isMetricsRecording) {
        if (currentMillis - ledBlinkTime > 500) {
            ledBlinkTime = currentMillis;
            ledState = !ledState;
            pixels.setPixelColor(0, ledState ? pixels.Color(0, 255, 255) : pixels.Color(0, 0, 0)); // Cyan
            pixels.show();
        }
        return;
    }
    if (strstr(local_status, "failed") != NULL || strstr(local_status, "Error") != NULL || strstr(local_status, "Timeout") != NULL || strstr(local_status, "Disconnected") != NULL) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
    } else if (WiFi.status() != WL_CONNECTED) {
        pixels.setPixelColor(0, pixels.Color(200, 200, 200)); // White
    } else {
        pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
    }
    pixels.show();
}

