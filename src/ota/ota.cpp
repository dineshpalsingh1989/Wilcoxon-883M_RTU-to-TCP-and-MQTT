#include "core/modules.h"

// =================================================================
// --- OTA CONFIGURATION
// =================================================================
void setupOTA() {
  ArduinoOTA.setHostname("ILA-Gateway-883M");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
    else type = "filesystem";
    logMessage("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    logMessage("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  });

  ArduinoOTA.onError([](ota_error_t error) {
    String err = "Error[" + String(error) + "]: ";
    if (error == OTA_AUTH_ERROR) err += "Auth Failed";
    else if (error == OTA_BEGIN_ERROR) err += "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) err += "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) err += "Receive Failed";
    else if (error == OTA_END_ERROR) err += "End Failed";
    logMessage(err);
  });

  ArduinoOTA.begin();
  logMessage("OTA Service Started");
}

// =================================================================
// --- FIRMWARE UPDATE HANDLERS (Manual OTA)
// =================================================================
void handleUpdateResult() {
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
        server.send(500, "text/html", "<h1>Update Failed.</h1><p>Device is rebooting with the old firmware. <a href='/'>Return</a> in 20 seconds.</p>");
    } else {
        server.send(200, "text/html", "<h1>Update Complete.</h1><p>Device is rebooting... <a href='/'>Click here to return</a> in 20 seconds.</p>");
    }
    delay(1000);
    ESP.restart();
}

void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        logMessage("Firmware Update Started: " + upload.filename);
        // Disable Watchdog for long updates prevents reboots during flash
        esp_task_wdt_deinit(); 
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Flashing firmware to ESP
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { // true to set the size to the current progress
            logMessage("Firmware Update Success: " + String(upload.totalSize) + " bytes.");
        } else {
            Update.printError(Serial);
            logMessage("Firmware Update Failed.");
        }
    }
}

