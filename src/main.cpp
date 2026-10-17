#include "core/modules.h"

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // 1. Initialize Mutex
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) { ESP.restart(); }

    // 2. Capture Reset Reason immediately
    lastResetReason = getResetReasonString();
    
    // 3. Init Hardware
    pixels.begin();
    pixels.setBrightness(40);
    pixels.show();
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // --- Boot/Reset Beep Sequence ---
    tone(BUZZER_PIN, 2000, 100); 
    delay(150);
    tone(BUZZER_PIN, 2500, 200); 
    delay(200);
    noTone(BUZZER_PIN);

    pinMode(USER_BUTTON_PIN, INPUT_PULLUP);

    // --- MAX485 Control Pin Setup ---
    pinMode(RS485_CTRL_PIN, OUTPUT);
    digitalWrite(RS485_CTRL_PIN, LOW); // Default to Receive Mode

    logMessage("--- System Starting ---");
    logMessage("Reset Reason: " + lastResetReason);

    // 4. Load Preferences
    preferences.begin("device-cfg", true); 
    dataCaptureInterval = preferences.getLong("wf_interval", 600000); 
    metricsReadInterval_ms = preferences.getLong("m_interval", 2000); 
    gmtOffset_sec = preferences.getLong("tz_offset", 28800); // Load Timezone
    preferences.end();
    
    preferences.begin("mqtt-cfg", true); 
    mqttEnabled = preferences.getBool("enabled", true);
    mqtt_use_tls = preferences.getBool("tls", true);
    mqtt_skip_cert_validation = preferences.getBool("skip_val", true);
    mqtt_publish_progress = preferences.getBool("progress", true);
    mqtt_server_str = preferences.getString("server", mqtt_server_str); 
    mqtt_port_val = preferences.getInt("port", mqtt_port_val); 
    mqtt_user_str = preferences.getString("user", mqtt_user_str); 
    mqtt_pass_str = preferences.getString("pass", mqtt_pass_str); 
    mqtt_topic_str = preferences.getString("topic", mqtt_topic_str); 
    preferences.end();

    preferences.begin("rs485-cfg", true); 
    modbus_slave_id_val = preferences.getUChar("slave_id", modbus_slave_id_val); 
    modbus_baud_rate_val = preferences.getLong("baud_rate", modbus_baud_rate_val); 
    preferences.end();
    
    preferences.begin("modbustcp-cfg", true);
    modbusTcpEnabled = preferences.getBool("enabled", true);
    modbusTcpPort = preferences.getUShort("port", 502);
    preferences.end();
    
    // 5. Connect WiFi
    String saved_ssid = "", saved_pass = "";
    preferences.begin("wifi-creds", true); saved_ssid = preferences.getString("ssid", ""); saved_pass = preferences.getString("password", ""); preferences.end();
    bool connected_to_wifi = false;
    
    if (saved_ssid.length() > 0) {
        logMessage("Connecting to Wi-Fi: " + saved_ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); attempts++; }
        if (WiFi.status() == WL_CONNECTED) { 
            connected_to_wifi = true; 
            logMessage("Wi-Fi Connected! IP: " + WiFi.localIP().toString());
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        } else { 
            logMessage("Wi-Fi Connect Failed. Starting AP Mode.");
            WiFi.disconnect(true); 
        }
    }
    if (!connected_to_wifi) { 
        WiFi.mode(WIFI_AP); 
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS); 
        logMessage("AP Mode Started: " + String(WIFI_AP_SSID));
    }

    wasWifiConnected = (WiFi.status() == WL_CONNECTED);

    // 5.1 Initialize OTA
    setupOTA();

    // 6. Init Modbus
    Serial1.begin(modbus_baud_rate_val, SERIAL_8N1, RXD_PIN, TXD_PIN);
    
    // --- Register MAX485 Callbacks ---
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    
    node.begin(modbus_slave_id_val, Serial1);
    
    // --- Call Custom Identity Function ---
    delay(500); 
    fetchSensorIdentity(); 

    if (connected_to_wifi && modbusTcpEnabled) {
      modbusTCPServer.server(modbusTcpPort);
      // 1. Add Live Metrics Registers (0-100)
      modbusTCPServer.addHreg(0, 0, 100); 
      // 2. Add Full Waveform Block (1000 - 14334)
      modbusTCPServer.addHreg(HREG_WAVEFORM_START, 0, WAVEFORM_TOTAL_POINTS);
      // 3. Add Full Spectrum Block (20000 - 26145)
      modbusTCPServer.addHreg(HREG_SPECTRUM_START, 0, SPECTRUM_TOTAL_POINTS);
      logMessage("Modbus TCP: Map 1000->Waveform, 20000->Spectrum");
    }
    
    // 7. Setup Web Server
    setupMonitoring();
    registerWebRoutes();
    
    server.begin();
    logMessage("Web Server Started");

    if(connected_to_wifi && mqttEnabled){
        if (mqtt_use_tls) {
            if (mqtt_skip_cert_validation) {
                espClientSecure.setInsecure();
            } 
            mqttClient = new PubSubClient(espClientSecure);
        } else {
            mqttClient = new PubSubClient(espClient);
        }

        if (mqttClient) {
            mqttClient->setServer(mqtt_server_str.c_str(), mqtt_port_val);
            mqttClient->setBufferSize(4096);
            mqttClient->setCallback(mqttCallback);
        } else {
             logMessage("FATAL: Failed to allocate MQTT client!");
        }
    }
    
    xTaskCreatePinnedToCore(webServerLoop, "WebServerTask", 12000, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(backgroundLoop, "BackgroundTask", 16384, NULL, 1, NULL, 1);
}

void loop() { vTaskDelete(NULL); }// End of Part 1. Handlers are in Part 2. 
