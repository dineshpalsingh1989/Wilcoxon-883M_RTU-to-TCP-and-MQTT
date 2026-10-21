#include "core/modules.h"
#if GATEWAY_ETHERNET
#include <LittleFS.h>
#endif

namespace {
bool manualAccepted = false, manualComplete = false;
void abortUpdate() {
#if GATEWAY_ETHERNET
    Update.end(false); // incomplete image: discarded, nothing is installed
#else
    Update.abort();
#endif
}
void releaseManualUpdate() {
    if (manualAccepted && xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        firmwareUpdateActive = false;
        xSemaphoreGive(dataMutex);
    }
    manualAccepted = false;
}
}

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
    if (!manualAccepted || !manualComplete || Update.hasError()) {
        releaseManualUpdate();
        server.send(409, "text/plain", "Update failed or device busy. No restart was performed."); return;
    }
    server.send(200, "text/plain", "Update Complete. Gateway restarting...");
    delay(1000);
    gw::restart();
}

void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        manualAccepted = false; manualComplete = false;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
        if (firmwareUpdateActive || currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum) { xSemaphoreGive(dataMutex); return; }
        firmwareUpdateActive = true; manualAccepted = true;
        xSemaphoreGive(dataMutex);
        const uint32_t start = millis(); bool paused = false;
        while (millis() - start < 15000) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) { paused = backgroundPausedForUpdate; xSemaphoreGive(dataMutex); }
            if (paused) break;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (!paused) { releaseManualUpdate(); return; }
        logMessage("Firmware Update Started: " + upload.filename);
#if GATEWAY_ETHERNET
        // The RP2350 stages the image in LittleFS and installs it at the next boot.
        FSInfo fs; LittleFS.info(fs);
        const size_t room = fs.totalBytes > fs.usedBytes + 16384 ? fs.totalBytes - fs.usedBytes - 16384 : 0;
        if (!Update.begin(room)) {
#else
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
#endif
            Update.printError(Serial);
            releaseManualUpdate();
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!manualAccepted) return;
        // Flashing firmware to ESP
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
            abortUpdate(); releaseManualUpdate();
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!manualAccepted) return;
        if (Update.end(true)) { // true to set the size to the current progress
            manualComplete = true;
            logMessage("Firmware Update Success: " + String(upload.totalSize) + " bytes.");
        } else {
            Update.printError(Serial);
            logMessage("Firmware Update Failed.");
            abortUpdate(); releaseManualUpdate();
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED && manualAccepted) {
        abortUpdate(); releaseManualUpdate();
    }
}

