#include "core/modules.h"

namespace {
enum class LedStatus { Off, Capture, Recording, Fault, Offline, Online };

void showStatus(LedStatus status) {
#ifdef STATUS_LED_PIN
    // Single-colour LED: lit while online or on a capture/recording blink; off for faults and offline.
    digitalWrite(STATUS_LED_PIN, status == LedStatus::Online || status == LedStatus::Capture || status == LedStatus::Recording ? HIGH : LOW);
#else
    uint32_t color = 0;
    switch (status) {
        case LedStatus::Capture:   color = pixels.Color(255, 255, 0); break;   // Yellow
        case LedStatus::Recording: color = pixels.Color(0, 255, 255); break;   // Cyan
        case LedStatus::Fault:     color = pixels.Color(255, 0, 0); break;     // Red
        case LedStatus::Offline:   color = pixels.Color(200, 200, 200); break; // White
        case LedStatus::Online:    color = pixels.Color(0, 255, 0); break;     // Green
        case LedStatus::Off:       break;
    }
    pixels.setPixelColor(0, color);
    pixels.show();
#endif
}
}

void initStatusLed() {
#ifdef STATUS_LED_PIN
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
#else
    pixels.begin();
    pixels.setBrightness(40);
    pixels.show();
#endif
}

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
            showStatus(ledState ? LedStatus::Capture : LedStatus::Off);
        }
        return;
    }
      if (local_isMetricsRecording) {
        if (currentMillis - ledBlinkTime > 500) {
            ledBlinkTime = currentMillis;
            ledState = !ledState;
            showStatus(ledState ? LedStatus::Recording : LedStatus::Off);
        }
        return;
    }
    if (strstr(local_status, "failed") != NULL || strstr(local_status, "Error") != NULL || strstr(local_status, "Timeout") != NULL || strstr(local_status, "Disconnected") != NULL) {
        showStatus(LedStatus::Fault);
    } else if (WiFi.status() != WL_CONNECTED) {
        showStatus(LedStatus::Offline);
    } else {
        showStatus(LedStatus::Online);
    }
}
