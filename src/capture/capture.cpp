#include "core/modules.h"

void logMetrics() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        MetricsSnapshot& entry = metricsLog[metricsLogIndex];
        strncpy(entry.timestamp, getTimestamp().c_str(), 20);
        
        entry.values[0] = accelRmsX; entry.values[1] = accelRmsY; entry.values[2] = accelRmsZ;
        entry.values[3] = accelPeakX; entry.values[4] = accelPeakY; entry.values[5] = accelPeakZ;
        entry.values[6] = veloRmsX; entry.values[7] = veloRmsY; entry.values[8] = veloRmsZ;
        entry.values[9] = veloPeakX; entry.values[10] = veloPeakY; entry.values[11] = veloPeakZ;
        entry.values[12] = displRmsX; entry.values[13] = displRmsY; entry.values[14] = displRmsZ;
        entry.values[15] = displPeakX; entry.values[16] = displPeakY; entry.values[17] = displPeakZ;
        entry.values[18] = truePeakX; entry.values[19] = truePeakY; entry.values[20] = truePeakZ;
        entry.values[21] = crestFactorX; entry.values[22] = crestFactorY; entry.values[23] = crestFactorZ;
        entry.values[24] = stdDeviationX; entry.values[25] = stdDeviationY; entry.values[26] = stdDeviationZ;
        entry.values[27] = accelRms2X; entry.values[28] = accelRms2Y; entry.values[29] = accelRms2Z;
        entry.values[30] = temperature;
        entry.values[31] = accelMetricRmsX; entry.values[32] = accelMetricRmsY; entry.values[33] = accelMetricRmsZ;
        entry.values[34] = accelMetricPeakX; entry.values[35] = accelMetricPeakY; entry.values[36] = accelMetricPeakZ;
        entry.values[37] = accelMetricRms2X; entry.values[38] = accelMetricRms2Y; entry.values[39] = accelMetricRms2Z;
        entry.values[40] = accelMetricPeak2X; entry.values[41] = accelMetricPeak2Y; entry.values[42] = accelMetricPeak2Z;
        
        entry.is_valid = true;
        
        metricsLogIndex = (metricsLogIndex + 1) % METRICS_LOG_SIZE;
        if (metricsLogCount < METRICS_LOG_SIZE) metricsLogCount++;
        
        xSemaphoreGive(dataMutex);
    }
}

void handleLongReads() {
    bool local_isReadingWaveform, local_isReadingSpectrum;
    Axis local_requestedAxis;
    bool local_isAutoCaptureCycleActive = false;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        local_isReadingWaveform = isReadingWaveform;
        local_isReadingSpectrum = isReadingSpectrum;
        local_requestedAxis = requestedAxis;
        local_isAutoCaptureCycleActive = isAutoCaptureCycleActive;
        xSemaphoreGive(dataMutex);
    } else { return; }

    char axisChar = (local_requestedAxis == AXIS_X) ? 'X' : (local_requestedAxis == AXIS_Y ? 'Y' : 'Z');

    if (local_isReadingWaveform) {
        uint16_t START_ADDRESS = 0;
        if (local_requestedAxis == AXIS_X) START_ADDRESS = ADDR_WAVE_X_START;
        else if (local_requestedAxis == AXIS_Y) START_ADDRESS = ADDR_WAVE_Y_START;
        else if (local_requestedAxis == AXIS_Z) START_ADDRESS = ADDR_WAVE_Z_START;
        else { 
            logMessage("[Waveform] Capture failed: Invalid Axis.");
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                isReadingWaveform = false;
                xSemaphoreGive(dataMutex);
            }
            return; 
        }

        bool success = readLargeDataBlock(START_ADDRESS, WAVEFORM_TOTAL_POINTS, fullWaveform);

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            isReadingWaveform = false; 
            if (success) {
                newWaveformAvailable = true;
                isReadingSpectrum = true; 
                
                // --- UPDATE MODBUS STATUS FOR PLC ---
                if(modbusTcpEnabled) {
                    uint16_t statusVal = 0;
                    if(local_requestedAxis == AXIS_X) statusVal = 2;
                    else if(local_requestedAxis == AXIS_Y) statusVal = 3;
                    else if(local_requestedAxis == AXIS_Z) statusVal = 4;
                    modbusTCPServer.Hreg(HREG_WAVEFORM_READY_STATUS, statusVal);
                }
                
                snprintf(systemStatus, sizeof(systemStatus), "Waveform (%c) Done. Reading Spectrum...", axisChar);
            } else {
                snprintf(systemStatus, sizeof(systemStatus), "Waveform Download Failed");
                recordCaptureResult(local_requestedAxis, "Waveform failed", "Not sent");
                strcpy(trafficLightStatus, "ERROR");
                currentDataState = DD_IDLE; 
                isAutoCaptureCycleActive = false;
            }
            xSemaphoreGive(dataMutex);
        }
        if (success && mqttClient && mqttClient->connected()) {
            publishDownsampledWaveform();
            publishFullWaveformInChunks(local_requestedAxis);
        }
    } 
    else if (local_isReadingSpectrum) {
        uint16_t START_ADDRESS = 0;
        if (local_requestedAxis == AXIS_X) START_ADDRESS = ADDR_SPEC_X_START;
        else if (local_requestedAxis == AXIS_Y) START_ADDRESS = ADDR_SPEC_Y_START;
        else if (local_requestedAxis == AXIS_Z) START_ADDRESS = ADDR_SPEC_Z_START;
        else { 
            logMessage("[Spectrum] Capture failed: Invalid Axis.");
             if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                isReadingSpectrum = false;
                xSemaphoreGive(dataMutex);
            }
            return; 
        }

        bool success = readLargeDataBlock(START_ADDRESS, SPECTRUM_TOTAL_POINTS, spectrumData);
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            isReadingSpectrum = false; 
            String ts = getTimestamp();
            if (success) {
                newSpectrumAvailable = true;
                
                // --- UPDATE MODBUS STATUS FOR PLC ---
                if(modbusTcpEnabled) {
                    uint16_t statusVal = 0;
                    if(local_requestedAxis == AXIS_X) statusVal = 2;
                    else if(local_requestedAxis == AXIS_Y) statusVal = 3;
                    else if(local_requestedAxis == AXIS_Z) statusVal = 4;
                    modbusTCPServer.Hreg(HREG_SPECTRUM_READY_STATUS, statusVal);
                }

                strncpy(lastCaptureTimestamp, ts.c_str(), sizeof(lastCaptureTimestamp) - 1);
                lastCapturedAxis = local_requestedAxis;
                snprintf(systemStatus, sizeof(systemStatus), "Capture complete for %c-axis.", axisChar);
                strcpy(trafficLightStatus, "READY"); 
                lastCaptureActionTime = millis();
                
                if (local_isAutoCaptureCycleActive) {
                    autoCaptureAxisIndex = (autoCaptureAxisIndex + 1) % 3;
                    if (autoCaptureAxisIndex != 0) { 
                        switch(autoCaptureAxisIndex) {
                            case 1: requestedAxis = AXIS_Y; break;
                            case 2: requestedAxis = AXIS_Z; break;
                        }
                        currentDataState = DD_REQUEST_DATA; 
                    } else { 
                        strncpy(lastAutoCaptureTimestamp, ts.c_str(), sizeof(lastAutoCaptureTimestamp) - 1);
                        lastAutoCaptureMillis = millis();
                        isAutoCaptureCycleActive = false;
                        currentDataState = DD_IDLE;
                        requestedCapture = CAPTURE_NONE;
                        requestedAxis = AXIS_NONE;
                    }
                } else {
                    currentDataState = DD_IDLE;
                    requestedCapture = CAPTURE_NONE;
                    requestedAxis = AXIS_NONE;
                }
                
            } else {
                snprintf(systemStatus, sizeof(systemStatus), "Spectrum Download Failed");
                recordCaptureResult(local_requestedAxis, "Spectrum failed", "Waveform may have been sent");
                strcpy(trafficLightStatus, "ERROR");
                currentDataState = DD_IDLE;
                isAutoCaptureCycleActive = false;
            }
            xSemaphoreGive(dataMutex); 
            
            if (success) {
                 if(mqttClient && mqttClient->connected()) {
                    publishFullSpectrumInChunks(local_requestedAxis);
                    
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        memcpy(globalScratchBuffer, spectrumData, SPECTRUM_TOTAL_POINTS * sizeof(int16_t));
                        xSemaphoreGive(dataMutex);
                    }
                    std::vector<SpectrumPeak> peaks = findSpectrumPeaks(globalScratchBuffer, 5);
                    
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        local_isAutoCaptureCycleActive = isAutoCaptureCycleActive;
                        xSemaphoreGive(dataMutex);
                    }
                    
                    if (local_isAutoCaptureCycleActive) {
                        xyzPeakDataCache[local_requestedAxis] = peaks;
                    } else {
                        publishSpectrumPeaks(local_requestedAxis, peaks);
                    }
                    
                    if(autoCaptureAxisIndex == 0 && xyzPeakDataCache.size() > 0) { 
                        publishCombinedSpectrumPeaks(); 
                    }
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        recordCaptureResult(local_requestedAxis, "Complete", "MQTT publish attempted");
                        xSemaphoreGive(dataMutex);
                    }
                } else {
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        recordCaptureResult(local_requestedAxis, "Complete", "MQTT offline - not sent");
                        xSemaphoreGive(dataMutex);
                    }
                }
            }
        }
    }
}

