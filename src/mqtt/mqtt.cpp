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
      data.add(fullWaveform[i] * scaleFactor);
    }
    xSemaphoreGive(dataMutex);
  } else { return; }

  char payload[6144];
  size_t n = serializeJson(doc, payload);
  mqttClient->publish(mqtt_topic_str.c_str(), payload, n);
}

void publishFullWaveformInChunks(Axis capturedAxis) {
    if (!mqttClient || !mqttClient->connected()) return;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        if (!newWaveformAvailable) {
            xSemaphoreGive(dataMutex);
            return;
        }
        memcpy(globalScratchBuffer, fullWaveform, WAVEFORM_TOTAL_POINTS * sizeof(int16_t));
        xSemaphoreGive(dataMutex);
    } else {
        return;
    }

    // ArduinoJson v6 needs ~16 bytes per array element on ESP32; 200 points fit the 4096-byte document and MQTT buffer.
    const int POINTS_PER_CHUNK = 200;
    const float scaleFactor = 0.001;
    String captureId = String(millis());
    int totalChunks = (WAVEFORM_TOTAL_POINTS + POINTS_PER_CHUNK - 1) / POINTS_PER_CHUNK;
    String axisStr = "N/A";
    switch(capturedAxis) {
        case AXIS_X: axisStr = "X"; break;
        case AXIS_Y: axisStr = "Y"; break;
        case AXIS_Z: axisStr = "Z"; break;
        default: break;
    }

    for (int i = 0; i < totalChunks; i++) {
        gw::watchdogFeed();
        
        DynamicJsonDocument doc(4096); 
        doc["model"] = sensor_model;
        doc["serial_number"] = sensor_serial;
        doc["timestamp"] = getTimestamp();
        doc["type"] = "waveform_chunk";
        doc["axis"] = axisStr;
        doc["capture_id"] = captureId;
        doc["chunk_index"] = i;
        doc["total_chunks"] = totalChunks;
        JsonArray data = doc.createNestedArray("data");
        int startIndex = i * POINTS_PER_CHUNK;
        int endIndex = min(startIndex + POINTS_PER_CHUNK, (int)WAVEFORM_TOTAL_POINTS);
        for (int j = startIndex; j < endIndex; j++) {
            data.add(globalScratchBuffer[j] * scaleFactor);
        }
        char payload[4096];
        size_t n = serializeJson(doc, payload);
        mqttClient->publish(mqtt_topic_str.c_str(), payload, n);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void publishFullSpectrumInChunks(Axis capturedAxis) {
    if (!mqttClient || !mqttClient->connected()) return;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        if (!newSpectrumAvailable) {
            xSemaphoreGive(dataMutex);
            return;
        }
        memcpy(globalScratchBuffer, spectrumData, SPECTRUM_TOTAL_POINTS * sizeof(int16_t));
        xSemaphoreGive(dataMutex);
    } else {
        return;
    }

    // ArduinoJson v6 needs ~16 bytes per array element on ESP32; 200 points fit the 4096-byte document and MQTT buffer.
    const int POINTS_PER_CHUNK = 200;
    const float scaleFactor = 0.001;
    String captureId = String(millis());
    int totalChunks = (SPECTRUM_TOTAL_POINTS + POINTS_PER_CHUNK - 1) / POINTS_PER_CHUNK;
    String axisStr = "N/A";
    switch(capturedAxis) {
        case AXIS_X: axisStr = "X"; break;
        case AXIS_Y: axisStr = "Y"; break;
        case AXIS_Z: axisStr = "Z"; break;
        default: break;
    }

    for (int i = 0; i < totalChunks; i++) {
        gw::watchdogFeed();
        
        DynamicJsonDocument doc(4096); 
        doc["model"] = sensor_model;
        doc["serial_number"] = sensor_serial;
        doc["timestamp"] = getTimestamp();
        doc["type"] = "spectrum_chunk";
        doc["axis"] = axisStr;
        doc["capture_id"] = captureId;
        doc["chunk_index"] = i;
        doc["total_chunks"] = totalChunks;
        JsonArray data = doc.createNestedArray("data");
        int startIndex = i * POINTS_PER_CHUNK;
        int endIndex = min(startIndex + POINTS_PER_CHUNK, (int)SPECTRUM_TOTAL_POINTS);
        for (int j = startIndex; j < endIndex; j++) {
            data.add(abs(globalScratchBuffer[j] * scaleFactor));
        }
        char payload[4096];
        size_t n = serializeJson(doc, payload);
        mqttClient->publish(mqtt_topic_str.c_str(), payload, n);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
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

