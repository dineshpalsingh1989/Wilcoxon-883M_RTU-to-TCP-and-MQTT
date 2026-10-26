#include "core/modules.h"

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // 1. Initialize Mutex
    gw::platformBegin();
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) { gw::restart(); }
    if (!allocateCaptureBuffers() || !allocateModbusTCPBuffers()) { gw::restart(); }

    // 2. Capture Reset Reason immediately
    lastResetReason = getResetReasonString();
    
    // 3. Init Hardware
    initStatusLed();
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // --- Boot/Reset Beep Sequence ---
    tone(BUZZER_PIN, 2000, 100); 
    delay(150);
    tone(BUZZER_PIN, 2500, 200); 
    delay(200);
    noTone(BUZZER_PIN);

    pinMode(USER_BUTTON_PIN, INPUT_PULLUP);

    // --- MAX485 Control Pin Setup (none on transceivers that switch direction themselves) ---
    if (RS485_CTRL_PIN >= 0) {
        pinMode(RS485_CTRL_PIN, OUTPUT);
        digitalWrite(RS485_CTRL_PIN, LOW); // Default to Receive Mode
    }

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
    mqtt_skip_cert_validation = preferences.getBool("skip_val", false);
    mqtt_publish_progress = preferences.getBool("progress", true);
    mqtt_server_str = preferences.getString("server", mqtt_server_str); 
    mqtt_port_val = preferences.getInt("port", mqtt_port_val);
    mqtt_user_str = preferences.getString("user", mqtt_user_str); 
    mqtt_pass_str = preferences.getString("pass", mqtt_pass_str); 
    mqtt_topic_str = preferences.getString("topic", mqtt_topic_str); 
    preferences.end();
#if GATEWAY_ETHERNET
    logMessage("[Beta] Watchdog disabled; networking runs in Arduino loop");
    // MQTT over TLS (8883) does not hold a session on the IRIV IO Controller yet (the broker closes
    // it about 2 s after connecting), so a TLS setting saved earlier is ignored: plain MQTT is used.
    if (mqtt_use_tls) {
        mqtt_use_tls = false;
        if (mqtt_port_val == 8883) mqtt_port_val = 1883;
        logMessage("[MQTT] TLS is not supported on this board; using plain MQTT on port " + String(mqtt_port_val));
    }
#endif

    preferences.begin("rs485-cfg", true); 
    modbus_slave_id_val = preferences.getUChar("slave_id", modbus_slave_id_val); 
    modbus_baud_rate_val = preferences.getLong("baud_rate", modbus_baud_rate_val); 
    preferences.end();
    
    preferences.begin("modbustcp-cfg", true);
    modbusTcpEnabled = preferences.getBool("enabled", true);
    modbusTcpPort = preferences.getUShort("port", 502);
    preferences.end();
    
    // 5. Connect the network
#if GATEWAY_ETHERNET
    const gw::EthernetAddress ethAddress = loadEthernetAddress();
    logMessage(ethAddress.useStatic ? "Starting Ethernet (static IP)..." : "Starting Ethernet (DHCP)...");
    if (gw::ethernetBegin(ethAddress)) logMessage("Ethernet IP: " + gw::netIP().toString());
    else logMessage("No DHCP answer. Ethernet uses fixed address " ETHERNET_FALLBACK_IP);
    gw::startTimeSync(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // Wired: start the servers now; they answer as soon as the cable is connected.
    bool connected_to_wifi = true;
#else
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
            gw::startTimeSync(gmtOffset_sec, daylightOffset_sec, ntpServer);
        } else { 
            logMessage("Wi-Fi Connect Failed. Starting AP Mode.");
            WiFi.disconnect(true); 
        }
    }
    if (!connected_to_wifi) { 
        WiFi.mode(WIFI_AP_STA); // STA side lets Wi-Fi scans run without dropping the access point
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS); 
        logMessage("AP Mode Started: " + String(WIFI_AP_SSID));
    }
#endif

    wasWifiConnected = gw::netConnected();

    // 5.1 Initialize OTA
    setupOTA();

    // 6. Init Modbus RTU (RS485 to the sensor)
    beginSensorRtu();
    delay(500);
    fetchSensorIdentity();

    // Modbus TCP for PLCs: 0-99 live metrics, 1000+ waveform, 20000+ spectrum.
    if (connected_to_wifi && modbusTcpEnabled) {
      if (beginModbusTCPServer(modbusTcpPort)) logMessage("Modbus TCP: Map 1000->Waveform, 20000->Spectrum");
      else logMessage("Modbus TCP: server failed to start on port " + String(modbusTcpPort));
    }
    
    // 7. Setup Web Server
    setupMonitoring();
#if GATEWAY_ETHERNET
    if (!io::begin()) logMessage("[I/O] Could not start the I/O sampler; relays stay off");
#endif
    registerWebRoutes();
    
    server.begin();
    logMessage("Web Server Started");

    if(connected_to_wifi && mqttEnabled){
        if (mqtt_use_tls) {
#if GATEWAY_ETHERNET
            setupSecureMqtt();
#else
            if (mqtt_skip_cert_validation) {
                espClientSecure.setInsecure();
            } 
            mqttClient = new WilcoxonMqttClient(espClientSecure);
#endif
        } else {
            mqttClient = new WilcoxonMqttClient(espClient);
#if GATEWAY_ETHERNET
            // Connect attempts block loop(), and with it the web server; fail an unreachable broker quickly.
            espClient.setTimeout(3000);
            if (mqttClient) mqttClient->setSocketTimeout(5);
#endif
        }

        if (mqttClient) {
            mqttClient->setServer(mqtt_server_str.c_str(), mqtt_port_val);
            mqttClient->setBufferSize(4096);
            mqttClient->setCallback(mqttCallback);
        } else {
             logMessage(String("[MQTT] ") + mqttStatus);
        }
    }
    
#if !GATEWAY_ETHERNET
    gw::startTask(webServerLoop, "WebServerTask", 12000, 1, 0);
    gw::startTask(backgroundLoop, "BackgroundTask", 16384, 1, 1);
#else
    gw::watchdogStart();
#endif
}

#if GATEWAY_ETHERNET
void loop() {
    webServerStep();
    backgroundStep();
    delay(10);
}
#else
void loop() { vTaskDelete(NULL); }
#endif
