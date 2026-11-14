#include "core/modules.h"
#if GATEWAY_ETHERNET
#include <LittleFS.h>
#endif

namespace {
bool manualAccepted = false, manualComplete = false;
// Why the last manual upload was refused or failed; sent back to the browser and logged.
String manualError;
String updaterError(const char* step) {
    static const char* const names[] = {"OK", "flash write", "flash erase", "read", "not enough space", "bad size",
        "stream", "MD5 mismatch", "flash config", "new flash config", "magic byte", "bootstrap", "signature", "no data"};
    const uint8_t code = Update.getError();
    return String(step) + " failed: " + (code < 14 ? names[code] : "unknown") + " (Updater error " + String(code) + ")";
}
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
        if (!manualError.length()) manualError = "the upload did not finish";
        logMessage("Firmware update not installed: " + manualError);
        server.send(409, "text/plain", "Update not installed: " + manualError + ". No restart was performed."); return;
    }
    server.send(200, "text/plain", "Update Complete. Gateway restarting...");
    delay(1000);
    gw::restart();
}

void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        manualAccepted = false; manualComplete = false; manualError = "";
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { manualError = "gateway busy, try again"; return; }
        if (firmwareUpdateActive || currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum || isPublishingCapture) {
            manualError = firmwareUpdateActive ? "another update is running" : "a capture or MQTT send is running; wait for it to finish";
            xSemaphoreGive(dataMutex); return;
        }
        firmwareUpdateActive = true; manualAccepted = true;
        xSemaphoreGive(dataMutex);
#if GATEWAY_ETHERNET
        // Upload handlers and backgroundStep run serially in Arduino loop.
        // No background operation can still be running while this handler runs.
        backgroundPausedForUpdate = true;
        bool paused = true;
#else
        const uint32_t start = millis(); bool paused = false;
        while (millis() - start < 15000) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) { paused = backgroundPausedForUpdate; xSemaphoreGive(dataMutex); }
            if (paused) break;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
#endif
        if (!paused) { manualError = "background work did not pause"; releaseManualUpdate(); return; }
        logMessage("Firmware Update Started: " + upload.filename);
#if GATEWAY_ETHERNET
        // The RP2350 stages the image in LittleFS and installs it at the next boot. A file left by an
        // aborted upload would otherwise count against the free space measured here.
        LittleFS.remove("firmware.bin");
        FSInfo fs; LittleFS.info(fs);
        const size_t room = fs.totalBytes > fs.usedBytes + 16384 ? fs.totalBytes - fs.usedBytes - 16384 : 0;
        if (!Update.begin(room)) {
            manualError = updaterError("Start") + ", " + String(room / 1024) + " KB free for staging";
#else
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
#endif
            Update.printError(Serial);
            if (!manualError.length()) manualError = updaterError("Start");
            releaseManualUpdate();
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!manualAccepted) return;
        // Flashing firmware to ESP
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
            manualError = updaterError("Write") + " at " + String(upload.totalSize / 1024) + " KB";
            abortUpdate(); releaseManualUpdate();
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!manualAccepted) return;
        if (Update.end(true)) { // true to set the size to the current progress
            manualComplete = true;
            logMessage("Firmware Update Success: " + String(upload.totalSize) + " bytes.");
        } else {
            Update.printError(Serial);
            manualError = updaterError("Finish");
            abortUpdate(); releaseManualUpdate();
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED && manualAccepted) {
        manualError = "the upload was interrupted";
        abortUpdate(); releaseManualUpdate();
    }
}

