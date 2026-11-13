#include "core/modules.h"

// =================================================================
// --- MQTT FUNCTIONS
// =================================================================
void publishDeviceHealth() {
    if (!mqttClient || !mqttClient->connected()) return;
    DynamicJsonDocument doc(2048); 
    doc["model"] = sensor_model; 
    doc["serial_number"] = sensor_serial; 
    doc["vendor_url"] = sensor_vendor_url; 
    doc["timestamp"] = getTimestamp();
    doc["type"] = "health_status";
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        doc["uptime"] = formatUptime(millis());
        doc["wifi_signal_rssi"] = gw::netRssi(); // 0 on wired Ethernet
        doc["free_heap_memory"] = gw::freeHeap();
        doc["reset_reason"] = lastResetReason;
        xSemaphoreGive(dataMutex);
    } else { return; }
    char payload[2048];
    size_t n = serializeJson(doc, payload);
    mqttClient->publish(mqtt_status_topic.c_str(), payload, n);
}

void publishMetrics() {
    if (!mqttClient || !mqttClient->connected()) return;
    DynamicJsonDocument doc(4096); 
    doc["model"] = sensor_model;
    doc["serial_number"] = sensor_serial;
    doc["vendor_url"] = sensor_vendor_url; 
    doc["timestamp"] = getTimestamp();
    doc["type"] = "metrics";
    JsonObject data = doc.createNestedObject("data");

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        JsonObject accelRms = data.createNestedObject("accelRms");
        accelRms["x"] = accelRmsX; accelRms["y"] = accelRmsY; accelRms["z"] = accelRmsZ;
        JsonObject accelPeak = data.createNestedObject("accelPeak");
        accelPeak["x"] = accelPeakX; accelPeak["y"] = accelPeakY; accelPeak["z"] = accelPeakZ;
        JsonObject accelRms2 = data.createNestedObject("accelRms2");
        accelRms2["x"] = accelRms2X; accelRms2["y"] = accelRms2Y; accelRms2["z"] = accelRms2Z;
        JsonObject accelPeak2 = data.createNestedObject("accelPeak2");
        accelPeak2["x"] = accelPeak2X; accelPeak2["y"] = accelPeak2Y; accelPeak2["z"] = accelPeak2Z;
        
        JsonObject amRms = data.createNestedObject("accelMetricRms");
        amRms["x"] = accelMetricRmsX; amRms["y"] = accelMetricRmsY; amRms["z"] = accelMetricRmsZ;
        JsonObject amPeak = data.createNestedObject("accelMetricPeak");
        amPeak["x"] = accelMetricPeakX; amPeak["y"] = accelMetricPeakY; amPeak["z"] = accelMetricPeakZ;
        JsonObject amRms2 = data.createNestedObject("accelMetricRms2");
        amRms2["x"] = accelMetricRms2X; amRms2["y"] = accelMetricRms2Y; amRms2["z"] = accelMetricRms2Z;
        JsonObject amPeak2 = data.createNestedObject("accelMetricPeak2");
        amPeak2["x"] = accelMetricPeak2X; amPeak2["y"] = accelMetricPeak2Y; amPeak2["z"] = accelMetricPeak2Z;

        JsonObject veloRms = data.createNestedObject("veloRms");
        veloRms["x"] = veloRmsX; veloRms["y"] = veloRmsY; veloRms["z"] = veloRmsZ;
        JsonObject veloPeak = data.createNestedObject("veloPeak");
        veloPeak["x"] = veloPeakX; veloPeak["y"] = veloPeakY; veloPeak["z"] = veloPeakZ;
        
        JsonObject vwRms = data.createNestedObject("veloWideRms");
        vwRms["x"] = veloWideRmsX; vwRms["y"] = veloWideRmsY; vwRms["z"] = veloWideRmsZ;
        JsonObject vwPeak = data.createNestedObject("veloWidePeak");
        vwPeak["x"] = veloWidePeakX; vwPeak["y"] = veloWidePeakY; vwPeak["z"] = veloWidePeakZ;
        
        JsonObject displRms = data.createNestedObject("displRms");
        displRms["x"] = displRmsX; displRms["y"] = displRmsY; displRms["z"] = displRmsZ;
        JsonObject displPeak = data.createNestedObject("displPeak");
        displPeak["x"] = displPeakX; displPeak["y"] = displPeakY; displPeak["z"] = displPeakZ;
        
        JsonObject dlRms = data.createNestedObject("displLowRms");
        dlRms["x"] = displLowRmsX; dlRms["y"] = displLowRmsY; dlRms["z"] = displLowRmsZ;
        JsonObject dlPeak = data.createNestedObject("displLowPeak");
        dlPeak["x"] = displLowPeakX; dlPeak["y"] = displLowPeakY; dlPeak["z"] = displLowPeakZ;

        JsonObject truePeak = data.createNestedObject("truePeak");
        truePeak["x"] = truePeakX; truePeak["y"] = truePeakY; truePeak["z"] = truePeakZ;
        JsonObject crestFactor = data.createNestedObject("crestFactor");
        crestFactor["x"] = crestFactorX; crestFactor["y"] = crestFactorY; crestFactor["z"] = crestFactorZ;
        JsonObject stdDeviation = data.createNestedObject("stdDeviation");
        stdDeviation["x"] = stdDeviationX; stdDeviation["y"] = stdDeviationY; stdDeviation["z"] = stdDeviationZ;
        
        data["temperature"] = temperature;
        xSemaphoreGive(dataMutex);
    } else { return; }
    char payload[4096];
    size_t n = serializeJson(doc, payload);
    mqttClient->publish(mqtt_topic_str.c_str(), payload, n);
}

void publishDownsampledWaveform() {
  if (!mqttClient || !mqttClient->connected()) return;
  DynamicJsonDocument doc(6144); 
  doc["model"] = sensor_model;
  doc["serial_number"] = sensor_serial;
  doc["timestamp"] = getTimestamp();
  doc["type"] = "waveform_preview";
  JsonArray data = doc.createNestedArray("data");
  const int DOWNSAMPLE_FACTOR = 100;
  const float scaleFactor = 0.001;

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (!newWaveformAvailable) {
        xSemaphoreGive(dataMutex);
        return;
    }
    for (int i = 0; i < WAVEFORM_TOTAL_POINTS; i += DOWNSAMPLE_FACTOR) {
      // Largest |sample| of each block, so short impacts are not skipped by the downsampling.
      int16_t peak = fullWaveform[i];
      for (int j = i + 1; j < i + DOWNSAMPLE_FACTOR && j < WAVEFORM_TOTAL_POINTS; j++) {
        if (abs(fullWaveform[j]) > abs(peak)) peak = fullWaveform[j];
      }
      data.add(peak * scaleFactor);
    }
    xSemaphoreGive(dataMutex);
  } else { return; }

  char payload[6144];
  size_t n = serializeJson(doc, payload);
  mqttClient->publish(mqtt_topic_str.c_str(), payload, n);
}

namespace {
// Writes raw * 0.001 exactly, without float rounding: -1234 -> "-1.234", 50 -> "0.05", 0 -> "0".
size_t formatMilli(int32_t raw, char* out) {
    size_t n = 0;
    if (raw < 0) { out[n++] = '-'; raw = -raw; }
    n += snprintf(out + n, 8, "%ld", (long)(raw / 1000));
    int frac = raw % 1000;
    if (frac) {
        out[n++] = '.';
        for (int div = 100; frac; div /= 10) { out[n++] = char('0' + frac / div); frac %= div; }
    }
    return n;
}

// Counts the JSON bytes (send = false) or sends them to the open MQTT publish in 1 KB pieces.
struct JsonSink {
    bool send;
    size_t total = 0;
    bool ok = true;
    char buf[1024];
    size_t used = 0;
    explicit JsonSink(bool sendIt) : send(sendIt) {}
    void put(const char* s, size_t n) {
        total += n;
        if (!send || !ok) return;
        if (used + n > sizeof(buf)) flush();
        memcpy(buf + used, s, n); // n is at most a header (< 512) or one number
        used += n;
    }
    bool flush() {
        if (send && ok && used) {
            ok = mqttClient->write(reinterpret_cast<const uint8_t*>(buf), used) == used;
            used = 0;
            gw::watchdogFeed();
        }
        return ok;
    }
};

// header is the message's JSON without "data"; the points go in a "data" array at its end.
void writeCaptureJson(JsonSink& out, const char* header, size_t headerLength, int points, bool isUnsigned) {
    out.put(header, headerLength - 1); // drop the closing '}'
    out.put(",\"data\":[", 9);
    char number[12];
    for (int i = 0; i < points; i++) {
        if (i) out.put(",", 1);
        // FFT registers are unsigned int16 (883M manual); waveform registers are signed.
        const int32_t raw = isUnsigned ? int32_t(uint16_t(globalScratchBuffer[i])) : int32_t(globalScratchBuffer[i]);
        out.put(number, formatMilli(raw, number));
    }
    out.put("]}", 2);
}

// Sends a whole waveform or spectrum (already copied to globalScratchBuffer) as ONE MQTT message.
// The JSON is formatted while it is sent, so it never has to fit in RAM (about 90 KB for a waveform).
// chunk_index 0 / total_chunks 1 keep subscribers written for the older 200-point chunks working.
bool publishWholeCapture(const char* type, Axis capturedAxis, int points, bool isUnsigned,
                         const char* stepKey, float stepValue) {
    DynamicJsonDocument doc(768);
    doc["model"] = sensor_model;
    doc["serial_number"] = sensor_serial;
    doc["timestamp"] = getTimestamp();
    doc["type"] = type;
    doc["unit"] = "g";
    doc[stepKey] = stepValue;
    doc["axis"] = capturedAxis == AXIS_X ? "X" : capturedAxis == AXIS_Y ? "Y" : capturedAxis == AXIS_Z ? "Z" : "N/A";
    // Unix seconds (once NTP has synced) keep the ID unique across reboots; uptime ms separates captures.
    doc["capture_id"] = String((unsigned long)time(nullptr)) + "-" + String(millis());
    doc["chunk_index"] = 0;
    doc["total_chunks"] = 1;
    doc["points"] = points;
    char header[512];
    const size_t headerLength = serializeJson(doc, header, sizeof(header));
    if (headerLength < 2 || headerLength >= sizeof(header) - 1) {
        logMessage(String("[MQTT] ") + type + " not sent: message header too long");
        return false;
    }

    JsonSink count(false); // MQTT needs the message length before the first payload byte
    writeCaptureJson(count, header, headerLength, points, isUnsigned);
    if (!mqttClient->beginPublish(mqtt_topic_str.c_str(), count.total, false)) {
        logMessage(String("[MQTT] ") + type + " not sent (" + String(count.total) + " bytes)");
        return false;
    }
    JsonSink out(true);
    writeCaptureJson(out, header, headerLength, points, isUnsigned);
    if (!out.flush() || !mqttClient->endPublish()) {
        // A message cut off part-way leaves the broker stream out of step, so start a fresh connection.
        mqttClient->disconnect();
        logMessage(String("[MQTT] ") + type + " send failed part-way (" + String(count.total) + " bytes); reconnecting");
        return false;
    }
    return true;
}

// Copies a finished capture under the mutex so the sensor task can start the next one meanwhile.
bool copyCapture(const int16_t* source, int points, const bool& available) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) != pdTRUE) return false;
    const bool ok = available;
    if (ok) memcpy(globalScratchBuffer, source, points * sizeof(int16_t));
    xSemaphoreGive(dataMutex);
    return ok;
}
} // namespace

bool publishFullWaveform(Axis capturedAxis) {
    if (!mqttClient || !mqttClient->connected()) return false;
    if (!copyCapture(fullWaveform, WAVEFORM_TOTAL_POINTS, newWaveformAvailable)) return false;
    return publishWholeCapture("waveform_chunk", capturedAxis, WAVEFORM_TOTAL_POINTS, false,
                               "sample_rate_hz", WAVEFORM_SAMPLE_RATE_HZ);
}

bool publishFullSpectrum(Axis capturedAxis) {
    if (!mqttClient || !mqttClient->connected()) return false;
    if (!copyCapture(spectrumData, SPECTRUM_TOTAL_POINTS, newSpectrumAvailable)) return false;
    return publishWholeCapture("spectrum_chunk", capturedAxis, SPECTRUM_TOTAL_POINTS, true,
                               "freq_resolution_hz", SPECTRUM_FREQ_RESOLUTION);
}

void publishSpectrumPeaks(Axis capturedAxis, const std::vector<SpectrumPeak>& peaks) {
    if (!mqttClient || !mqttClient->connected()) return;
    DynamicJsonDocument doc(2048);
    doc["model"] = sensor_model;
    doc["serial_number"] = sensor_serial;
    doc["timestamp"] = getTimestamp();
    doc["type"] = "spectrum_peaks";
    String axisStr = "N/A";
    switch(capturedAxis) {
        case AXIS_X: axisStr = "X"; break;
        case AXIS_Y: axisStr = "Y"; break;
        case AXIS_Z: axisStr = "Z"; break;
        default: break;
    }
    doc["axis"] = axisStr;
    JsonArray peaks_array = doc.createNestedArray("peaks");
    for (const auto& peak : peaks) {
        JsonObject p = peaks_array.createNestedObject();
        p["freq_hz"] = round(peak.frequency_hz * 100) / 100.0; 
        p["amp_g"] = round(peak.amplitude_g * 10000) / 10000.0; 
    }
    char payload[2048];
    size_t n = serializeJson(doc, payload);
    mqttClient->publish(mqtt_topic_str.c_str(), payload, n);
}

void publishCombinedSpectrumPeaks() {
    if (!mqttClient || !mqttClient->connected() || xyzPeakDataCache.empty()) return;
    DynamicJsonDocument doc(4096);
    doc["model"] = sensor_model;
    doc["serial_number"] = sensor_serial;
    doc["timestamp"] = getTimestamp();
    doc["type"] = "combined_spectrum_peaks";
    JsonObject peaks_obj = doc.createNestedObject("peaks");
    for (auto const& [axis, peaks] : xyzPeakDataCache) {
        String axisStr = (axis == AXIS_X) ? "X" : (axis == AXIS_Y) ? "Y" : "Z";
        JsonArray peaks_array = peaks_obj.createNestedArray(axisStr);
        for (const auto& peak : peaks) {
            JsonObject p = peaks_array.createNestedObject();
            p["freq_hz"] = round(peak.frequency_hz * 100) / 100.0;
            p["amp_g"] = round(peak.amplitude_g * 10000) / 10000.0;
        }
    }
    char payload[4096];
    size_t n = serializeJson(doc, payload);
    mqttClient->publish(mqtt_status_topic.c_str(), payload, n);
    xyzPeakDataCache.clear();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topic_str(topic);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload, length);
    
    const char* target_sn = doc["serial_number"];
    
    // --- STRICT SERIAL NUMBER MATCHING LOGIC ---
    if (target_sn && String(target_sn) == sensor_serial) {
        
        const char* command = doc["command"];
        if (command) {
            logMessage("MQTT Command received: " + String(command));
            
            if (strcmp(command, "get_metrics") == 0) {
                publishMetrics();
                return;
            }
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                bool captureBusy = firmwareUpdateActive || (currentDataState != DD_IDLE) || isReadingWaveform || isReadingSpectrum;
                if ((strcmp(command, "trigger_all") == 0 || strcmp(command, "trigger_capture") == 0) && captureBusy) {
                    Serial.println("MQTT: capture ignored, device busy");
                }
                else if (strcmp(command, "trigger_all") == 0) {
                    requestedAxis = AXIS_X;                 
                    requestedCapture = CAPTURE_BOTH;        
                    currentDataState = DD_REQUEST_DATA; 
                    
                    isAutoCaptureCycleActive = true;        
                    autoCaptureAxisIndex = 0;               
                    xyzPeakDataCache.clear();               
                    
                    logMessage("MQTT: Starting Full XYZ Capture Sequence");
                }
                else if (strcmp(command, "trigger_capture") == 0) {
                    String axis = doc["axis"] | "X";
                    if(axis == "X") requestedAxis = AXIS_X;
                    else if(axis == "Y") requestedAxis = AXIS_Y;
                    else if(axis == "Z") requestedAxis = AXIS_Z;
                    
                    if(requestedAxis != AXIS_NONE) {
                        requestedCapture = CAPTURE_BOTH;
                        currentDataState = DD_REQUEST_DATA;
                        isAutoCaptureCycleActive = false; 
                        logMessage("MQTT: Triggering Single Axis " + axis);
                    }
                } 
                else if (strcmp(command, "set_interval") == 0) {
                    long newInterval = doc["interval_ms"];
                    if (newInterval > 0) {
                        dataCaptureInterval = newInterval;
                        preferences.begin("device-cfg", false);
                        preferences.putLong("wf_interval", dataCaptureInterval);
                        preferences.end();
                        logMessage("Interval updated via MQTT to " + String(newInterval));
                    }
                } 
                xSemaphoreGive(dataMutex);
            }
            if (strcmp(command, "reboot") == 0) { 
                logMessage("Remote reboot requested. Restarting..."); 
                shouldReboot = true; 
            } 
        }
    } else {
        String receivedSN = target_sn ? String(target_sn) : "MISSING";
        logMessage("[MQTT] Command IGNORED. SN mismatch. Received: " + receivedSN + ", Device: " + sensor_serial);
    }
}

