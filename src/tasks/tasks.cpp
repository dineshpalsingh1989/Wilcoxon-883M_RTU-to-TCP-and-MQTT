#include "core/modules.h"

// =================================================================
// --- RTOS TASKS ---
// =================================================================
void webServerLoop(void * pvParameters) {
    for(;;) { 
        server.handleClient(); 
        bool updating = true;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            updating = firmwareUpdateActive;
            xSemaphoreGive(dataMutex);
        }
        if (!updating) ArduinoOTA.handle();
        vTaskDelay(2); 
    }
}

void backgroundLoop(void * pvParameters) {
    gw::watchdogStart();
    
    for(;;) {
        // --- FIX: Feed watchdog every loop iteration ---
        gw::watchdogFeed();
        bool updating = false;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) { vTaskDelay(10); continue; }
        updating = firmwareUpdateActive;
        backgroundPausedForUpdate = updating;
        xSemaphoreGive(dataMutex);
        if (updating) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        // --- NEW: Safe Reboot Handler ---
        if (shouldReboot) {
            vTaskDelay(pdMS_TO_TICKS(500));
            gw::restart();
        }
        // --------------------------------

        bool isWifiConnected = gw::netConnected();

        if (digitalRead(USER_BUTTON_PIN) == LOW) { 
            if (buttonPressStartTime == 0) {
                buttonPressStartTime = millis();
            }
            if (millis() - buttonPressStartTime > 5000 && !apModeTriggeredByButton) {
                apModeTriggeredByButton = true;
#if GATEWAY_ETHERNET
                logMessage("Fixed address " ETHERNET_FALLBACK_IP " forced by button press");
                gw::ethernetUseFallback();
#else
                logMessage("AP Mode Forced by Button Press");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_AP_STA); // keeps Wi-Fi scans from dropping the access point
                WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
#endif
                tone(BUZZER_PIN, 2000, 500);
            }
        } else {
            buttonPressStartTime = 0;
            apModeTriggeredByButton = false;
        }

        if (wasWifiConnected && !isWifiConnected && !apModeTriggeredByButton) {
            wifiDisconnectAlarmActive = true;
            lastAlarmActionTime = millis();
            isAlarmBeeping = true;
            tone(BUZZER_PIN, 1200);
#if GATEWAY_ETHERNET
            snprintf(systemStatus, sizeof(systemStatus), "Ethernet Disconnected!");
            logMessage("[Ethernet] Warning: Connection Lost!");
#else
            snprintf(systemStatus, sizeof(systemStatus), "Wi-Fi Disconnected!");
            logMessage("[WiFi] Warning: Connection Lost!");
#endif
        } else if (!wasWifiConnected && isWifiConnected) {
            wifiDisconnectAlarmActive = false;
            noTone(BUZZER_PIN);
            logMessage("[Network] Reconnected. IP: " + gw::netIP().toString());
            gw::startTimeSync(gmtOffset_sec, daylightOffset_sec, ntpServer); // Resync time
        }
        wasWifiConnected = isWifiConnected;
        
        if (wifiDisconnectAlarmActive) {
            if (isAlarmBeeping && (millis() - lastAlarmActionTime >= 3000)) {
                isAlarmBeeping = false;
                noTone(BUZZER_PIN);
                lastAlarmActionTime = millis();
            } else if (!isAlarmBeeping && (millis() - lastAlarmActionTime >= 10000)) {
                isAlarmBeeping = true;
                tone(BUZZER_PIN, 1200);
                lastAlarmActionTime = millis();
            }
        }
        
        if (mqttEnabled && isWifiConnected && mqttClient) {
            if (!mqttClient->connected()) {
                if (millis() - lastMqttAttemptTime > mqttReconnectInterval) {
                    lastMqttAttemptTime = millis();
                    
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        snprintf(mqttStatus, sizeof(mqttStatus), "Connecting...");
                        xSemaphoreGive(dataMutex);
                    }
                    
                    gw::watchdogFeed();

                    String clientId = "esp32-ila-" + gw::deviceId(); // unique per device so gateways don't kick each other off
                    logMessage("[MQTT] Connecting to " + mqtt_server_str + ":" + String(mqtt_port_val) + (mqtt_use_tls ? " (TLS)" : ""));
                    if (mqttClient->connect(clientId.c_str(), mqtt_user_str.c_str(), mqtt_pass_str.c_str())) {
                         logMessage("[MQTT] Connected to " + mqtt_server_str);
                         if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            snprintf(mqttStatus, sizeof(mqttStatus), "Connected");
                            xSemaphoreGive(dataMutex);
                         }
                        mqttClient->subscribe(mqtt_command_topic.c_str());
                    } else {
                         logMessage("[MQTT] Connect Failed rc=" + String(mqttClient->state()));
#if GATEWAY_ETHERNET
                         char tlsError[96];
                         if (mqtt_use_tls && espClientSecure.getLastSSLError(tlsError, sizeof(tlsError)))
                             logMessage(String("[MQTT] TLS: ") + tlsError);
#endif
                    }
                }
            }
            if (mqttClient->connected()) { 
                mqttClient->loop(); 
                if (millis() - lastHealthPublishTime >= healthPublishInterval) {
                    lastHealthPublishTime = millis();
                    publishDeviceHealth();
                }
            }
        }

        pollModbusTCPServer();
        updateLedStatus();
        
        handleLongReads(); // Handles the actual Modbus reading state machine

        // Sync Modbus TCP registers if new data arrived
        static unsigned long lastModbusSync = 0;
        if (millis() - lastModbusSync > 1000) { 
             syncModbusRegisters();
             lastModbusSync = millis();
        }

        bool local_isReadingWaveform, local_isReadingSpectrum;
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            local_isReadingWaveform = isReadingWaveform;
            local_isReadingSpectrum = isReadingSpectrum;
            xSemaphoreGive(dataMutex);
        } else { continue; }

        if (!local_isReadingWaveform && !local_isReadingSpectrum) {
            if (millis() - lastMetricsReadTime >= metricsReadInterval_ms) { 
                lastMetricsReadTime = millis();
                bool read_ok = true;
                
                // Temporary buffers
                float t_dx = 0, t_dy = 0, t_dz = 0; // TODO: displacement RMS is never read from the sensor (register address unknown)
                float t_ax, t_ay, t_az, t_apx, t_apy, t_apz, t_ax2, t_ay2, t_az2, t_apx2, t_apy2, t_apz2;
                float t_vx, t_vy, t_vz, t_vpx, t_vpy, t_vpz, t_dpx, t_dpy, t_dpz;
                float t_tpx, t_tpy, t_tpz, t_cfx, t_cfy, t_cfz, t_sdx, t_sdy, t_sdz, t_t;
                float t_mamx, t_mamy, t_mamz, t_mapx, t_mapy, t_mapz, t_mam2x, t_mam2y, t_mam2z, t_map2x, t_map2y, t_map2z;
                float t_vwx, t_vwy, t_vwz, t_vwpx, t_vwpy, t_vwpz;
                float t_dlx, t_dly, t_dlz, t_dlpx, t_dlpy, t_dlpz;

                uint8_t result;
                uint16_t rb[3]; // Raw registers from the last read

                // --- CRITICAL WDT FIX: Fail Fast on First Read ---
                result = readSensorRegisters(0x0000, 3, rb);
                if (result == 0) {
                    t_ax = (int16_t)rb[0]/100.0;
                    t_ay = (int16_t)rb[1]/100.0;
                    t_az = (int16_t)rb[2]/100.0;
                } else {
                    read_ok = false;
                }
                
                if (read_ok) { // Only proceed if first read worked
                    vTaskDelay(5);
                    result = readSensorRegisters(0x0004, 3, rb); if(result == 0) { t_apx=(int16_t)rb[0]/100.0; t_apy=(int16_t)rb[1]/100.0; t_apz=(int16_t)rb[2]/100.0;} else read_ok=false; vTaskDelay(5);
                    if(read_ok) { result = readSensorRegisters(0x0008, 3, rb); if(result == 0) { t_mamx=(int16_t)rb[0]/10.0; t_mamy=(int16_t)rb[1]/10.0; t_mamz=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x000C, 3, rb); if(result == 0) { t_mapx=(int16_t)rb[0]/10.0; t_mapy=(int16_t)rb[1]/10.0; t_mapz=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0010, 3, rb); if(result == 0) { t_ax2=(int16_t)rb[0]/100.0; t_ay2=(int16_t)rb[1]/100.0; t_az2=(int16_t)rb[2]/100.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0014, 3, rb); if(result == 0) { t_apx2=(int16_t)rb[0]/100.0; t_apy2=(int16_t)rb[1]/100.0; t_apz2=(int16_t)rb[2]/100.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0018, 3, rb); if(result == 0) { t_mam2x=(int16_t)rb[0]/10.0; t_mam2y=(int16_t)rb[1]/10.0; t_mam2z=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x001C, 3, rb); if(result == 0) { t_map2x=(int16_t)rb[0]/10.0; t_map2y=(int16_t)rb[1]/10.0; t_map2z=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0020, 3, rb); if(result == 0) { t_vx=(int16_t)rb[0]/10.0; t_vy=(int16_t)rb[1]/10.0; t_vz=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0024, 3, rb); if(result == 0) { t_vpx=(int16_t)rb[0]/10.0; t_vpy=(int16_t)rb[1]/10.0; t_vpz=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0028, 3, rb); if(result == 0) { t_vwx=(int16_t)rb[0]/10.0; t_vwy=(int16_t)rb[1]/10.0; t_vwz=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x002C, 3, rb); if(result == 0) { t_vwpx=(int16_t)rb[0]/10.0; t_vwpy=(int16_t)rb[1]/10.0; t_vwpz=(int16_t)rb[2]/10.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0030, 3, rb); if(result == 0) { t_dlx=(int16_t)rb[0]/1.0; t_dly=(int16_t)rb[1]/1.0; t_dlz=(int16_t)rb[2]/1.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0034, 3, rb); if(result == 0) { t_dlpx=(int16_t)rb[0]/1.0; t_dlpy=(int16_t)rb[1]/1.0; t_dlpz=(int16_t)rb[2]/1.0; t_dpx=(int16_t)rb[0]/1.0; t_dpy=(int16_t)rb[1]/1.0; t_dpz=(int16_t)rb[2]/1.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0040, 3, rb); if(result == 0) { t_tpx=(int16_t)rb[0]/100.0; t_tpy=(int16_t)rb[1]/100.0; t_tpz=(int16_t)rb[2]/100.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0044, 3, rb); if(result == 0) { t_cfx=(int16_t)rb[0]/100.0; t_cfy=(int16_t)rb[1]/100.0; t_cfz=(int16_t)rb[2]/100.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0048, 3, rb); if(result == 0) { t_sdx=(int16_t)rb[0]/100.0; t_sdy=(int16_t)rb[1]/100.0; t_sdz=(int16_t)rb[2]/100.0;} else read_ok=false; vTaskDelay(5); }
                    if(read_ok) { result = readSensorRegisters(0x0050, 1, rb); if(result == 0) t_t = (int16_t)rb[0]/10.0; else read_ok = false; }
                }

                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (read_ok) {
                        // Standard Accel
                        accelRmsX = t_ax; accelRmsY = t_ay; accelRmsZ = t_az;
                        accelPeakX = t_apx; accelPeakY = t_apy; accelPeakZ = t_apz;
                        accelRms2X = t_ax2; accelRms2Y = t_ay2; accelRms2Z = t_az2;
                        accelPeak2X = t_apx2; accelPeak2Y = t_apy2; accelPeak2Z = t_apz2;
                        // New Metric Accel
                        accelMetricRmsX = t_mamx; accelMetricRmsY = t_mamy; accelMetricRmsZ = t_mamz;
                        accelMetricPeakX = t_mapx; accelMetricPeakY = t_mapy; accelMetricPeakZ = t_mapz;
                        accelMetricRms2X = t_mam2x; accelMetricRms2Y = t_mam2y; accelMetricRms2Z = t_mam2z;
                        accelMetricPeak2X = t_map2x; accelMetricPeak2Y = t_map2y; accelMetricPeak2Z = t_map2z;
                        // Standard Velo
                        veloRmsX = t_vx; veloRmsY = t_vy; veloRmsZ = t_vz;
                        veloPeakX = t_vpx; veloPeakY = t_vpy; veloPeakZ = t_vpz;
                        // New Wide Velo
                        veloWideRmsX = t_vwx; veloWideRmsY = t_vwy; veloWideRmsZ = t_vwz;
                        veloWidePeakX = t_vwpx; veloWidePeakY = t_vwpy; veloWidePeakZ = t_vwpz;
                        // Standard Disp
                        displRmsX = t_dx; displRmsY = t_dy; displRmsZ = t_dz;
                        displPeakX = t_dpx; displPeakY = t_dpy; displPeakZ = t_dpz;
                        // New Low Disp
                        displLowRmsX = t_dlx; displLowRmsY = t_dly; displLowRmsZ = t_dlz;
                        displLowPeakX = t_dlpx; displLowPeakY = t_dlpy; displLowPeakZ = t_dlpz;
                        // Other
                        truePeakX = t_tpx; truePeakY = t_tpy; truePeakZ = t_tpz;
                        crestFactorX = t_cfx; crestFactorY = t_cfy; crestFactorZ = t_cfz;
                        stdDeviationX = t_sdx; stdDeviationY = t_sdy; stdDeviationZ = t_sdz;
                        temperature = t_t;
                        recordMetricsHealth(true, 0);
                        
                        if (!wifiDisconnectAlarmActive) {
                            snprintf(systemStatus, sizeof(systemStatus), "Monitoring");
                        }
                        bool local_isMetricsRecording = isMetricsRecording;
                        xSemaphoreGive(dataMutex);
                        
                        if(mqttClient && mqttClient->connected()) { publishMetrics(); }
                        if(local_isMetricsRecording) { logMetrics(); }
                        if (modbusTcpEnabled) { updateModbusTCPCache(); syncModbusRegisters(); } // Use syncModbusRegisters
                    } else {
                        // --- FAULT HANDLING ---
                        recordMetricsHealth(false, result);
                        snprintf(systemStatus, sizeof(systemStatus), "Sensor Disconnected");
                        logMessage("Error reading metrics: Sensor likely disconnected.");
                        strcpy(trafficLightStatus, "ERROR"); 
                        xSemaphoreGive(dataMutex);
                    }
                }
            }
            
            DynamicDataState local_currentDataState;
            long local_dataCaptureInterval;
            unsigned long local_lastAutoCaptureMillis;

            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                local_currentDataState = currentDataState;
                local_dataCaptureInterval = dataCaptureInterval;
                local_lastAutoCaptureMillis = lastAutoCaptureMillis;
                xSemaphoreGive(dataMutex);
            } else { continue; }

            switch (local_currentDataState) {
                case DD_IDLE:
                    if (local_dataCaptureInterval < INTERVAL_NEVER && (millis() - local_lastAutoCaptureMillis >= local_dataCaptureInterval)) {
                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            if (firmwareUpdateActive) { xSemaphoreGive(dataMutex); break; }
                            requestedCapture = CAPTURE_BOTH;
                            isAutoCaptureCycleActive = true;
                            autoCaptureAxisIndex = 0; 
                            requestedAxis = AXIS_X;
                            currentDataState = DD_REQUEST_DATA;
                            xyzPeakDataCache.clear(); 
                            xSemaphoreGive(dataMutex);
                        }
                    }
                    break;
                
                case DD_REQUEST_DATA:
                    if (requestNewWaveform()) {
                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            snprintf(systemStatus, sizeof(systemStatus), "Sensor Calculating...");
                            strcpy(trafficLightStatus, "TRIGGERED"); 
                            pollAttemptCounter = 0; 
                            lastPollTime = millis(); 
                            currentDataState = DD_WAITING_FOR_DATA;
                            xSemaphoreGive(dataMutex);
                        }
                    } else {
                        logMessage("[Sensor] Failed to request new data (Trigger failed).");
                        lastAutoCaptureMillis = millis(); // back off until the next interval instead of retrying every loop
                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            snprintf(systemStatus, sizeof(systemStatus), "Failed to request data");
                            recordCaptureResult(requestedAxis, "Trigger failed", "Not sent");
                            strcpy(trafficLightStatus, "ERROR"); 
                            lastCaptureActionTime = millis(); 
                            currentDataState = DD_IDLE;
                            requestedCapture = CAPTURE_NONE;
                            isAutoCaptureCycleActive = false;
                            xSemaphoreGive(dataMutex);
                        }
                    }
                    break;

                case DD_WAITING_FOR_DATA:
                    if (millis() - lastPollTime >= dataPollInterval) { 
                        lastPollTime = millis();
                        
                        if (isWaveformReady()) { 
                            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                snprintf(systemStatus, sizeof(systemStatus), "Sensor Ready. Downloading...");
                                strcpy(trafficLightStatus, "READY"); 
                                isReadingWaveform = true; 
                                waveformReadIndex = 0;
                                spectrumReadIndex = 0;
                                newWaveformAvailable = false;
                                newSpectrumAvailable = false;
                                currentDataState = DD_IDLE; 
                                xSemaphoreGive(dataMutex);
                            }
                        } else if (pollAttemptCounter >= maxPollAttempts) {
                            logMessage("[Sensor] Timeout waiting for 0x0200 ready flag.");
                            lastAutoCaptureMillis = millis();
                            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                snprintf(systemStatus, sizeof(systemStatus), "Timeout waiting for sensor");
                                recordCaptureResult(requestedAxis, "Sensor timeout", "Not sent");
                                strcpy(trafficLightStatus, "ERROR"); 
                                lastCaptureActionTime = millis(); 
                                currentDataState = DD_IDLE;
                                requestedCapture = CAPTURE_NONE;
                                isAutoCaptureCycleActive = false;
                                xSemaphoreGive(dataMutex);
                            }
                        } else {
                            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                snprintf(systemStatus, sizeof(systemStatus), "Waiting for sensor... (Poll %d/%d)", pollAttemptCounter, maxPollAttempts);
                                strcpy(trafficLightStatus, "BUSY"); 
                                pollAttemptCounter++;
                                xSemaphoreGive(dataMutex);
                            }
                        }
                    }
                    break;
            }
        }
        vTaskDelay(10);
    }
}

