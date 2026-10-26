#include "core/modules.h"

namespace {
uint8_t errorCode(Wilcoxon883MRTU::Result result) {
    switch (result) {
        case Wilcoxon883MRTU::Result::Ok: return 0;
        case Wilcoxon883MRTU::Result::Exception: return sensorRtu.exceptionCode();
        case Wilcoxon883MRTU::Result::Timeout: return RTU_ERROR_TIMEOUT;
        case Wilcoxon883MRTU::Result::Crc: return RTU_ERROR_CRC;
        case Wilcoxon883MRTU::Result::Frame: return RTU_ERROR_INVALID_RESPONSE;
        default: return RTU_ERROR_GATEWAY;
    }
}
}

void beginSensorRtu() {
    // The library drives DE/RE on RS485_CTRL_PIN around each request.
    if (!sensorRtu.begin(RXD_PIN, TXD_PIN, RS485_CTRL_PIN, modbus_baud_rate_val, modbus_slave_id_val, SERIAL_8N1)) {
        logMessage("[Modbus] RS485 setup failed: check baud rate (" + String(modbus_baud_rate_val) + ") and slave ID");
    }
}

// =================================================================
// --- MODBUS IDENTITY FUNCTION (function 43 / MEI 14)
// =================================================================
void fetchSensorIdentity() {
    logMessage("[System] Fetching Sensor Identity...");
    Wilcoxon883MRTU::Identity identity;
    Wilcoxon883MRTU::Result result = sensorRtu.readIdentity(identity, 1000);
    if (result != Wilcoxon883MRTU::Result::Ok) {
        logMessage(String("[System] Identity read failed: ") + Wilcoxon883MRTU::resultText(result));
        return;
    }
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    // Object IDs as used before: 1 product code (used as serial for MQTT commands),
    // 2 revision, 3 vendor URL, 4 product name (part number), 5 model.
    sensor_serial = identity.product;
    sensor_fw_version = identity.revision;
    sensor_vendor_url = identity.vendorUrl;
    sensor_part_number = identity.productName;
    sensor_model = identity.model;
    String summary = "[System] ID Result -> Model: " + sensor_model + " | Serial: " + sensor_serial + " | PN: " + sensor_part_number;
    xSemaphoreGive(dataMutex);
    logMessage(summary);
}

bool requestNewWaveform() {
    return sensorRtu.writeSingleRegister(REG_TRIGGER, VAL_TRIGGER_START) == Wilcoxon883MRTU::Result::Ok;
}

bool isWaveformReady() {
    uint16_t status = 0;
    return sensorRtu.readHoldingRegisters(REG_TRIGGER, 1, &status, 1) == Wilcoxon883MRTU::Result::Ok &&
           status == VAL_STATUS_READY;
}

uint8_t readSensorRegisters(uint16_t address, uint16_t count, uint16_t* values) {
    return errorCode(sensorRtu.readInputRegisters(address, count, values, count));
}

bool readSensorValue(const char* name, uint16_t address, float scale, float &target) {
    uint16_t raw = 0;
    if (readSensorRegisters(address, 1, &raw) != 0) return false;
    target = (int16_t)raw / scale;
    return true;
}

bool readLargeDataBlock(uint16_t startAddress, uint16_t totalPoints, int16_t* buffer) {
    uint16_t pointsRead = 0;
    const uint16_t chunkSize = Wilcoxon883MRTU::MaxRegisters;
    uint16_t chunk[Wilcoxon883MRTU::MaxRegisters];
    int retryCount = 0;
    const int maxRetries = 3;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        snprintf(systemStatus, sizeof(systemStatus), "Downloading... (0%%)");
        strcpy(trafficLightStatus, "BUSY");
        xSemaphoreGive(dataMutex);
    }

    while (pointsRead < totalPoints) {
        gw::watchdogFeed();
        pollModbusTCPServer();
#if GATEWAY_ETHERNET
        webServerStep(); // loop() also runs the web server here, so keep pages answering during a download
#endif

        uint16_t pointsLeft = totalPoints - pointsRead;
        uint16_t pointsToRead = (pointsLeft > chunkSize) ? chunkSize : pointsLeft;
        bool stored = false;
        if (readSensorRegisters(startAddress + pointsRead, pointsToRead, chunk) == 0 &&
            xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < pointsToRead; i++) buffer[pointsRead + i] = (int16_t)chunk[i];
            xSemaphoreGive(dataMutex);
            stored = true;
        }

        if (stored) {
            pointsRead += pointsToRead;
            retryCount = 0;

            if (pointsRead % 1000 < chunkSize || pointsRead == totalPoints) {
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    snprintf(systemStatus, sizeof(systemStatus), "Downloading... (%d%%)", (pointsRead * 100) / totalPoints);
                    xSemaphoreGive(dataMutex);
                }
                if (mqttClient && mqttClient->connected()) mqttClient->loop();
                gw::watchdogFeed();
            }
        } else {
            // A failed read or a busy mutex retries the same chunk, so no gap is left in the buffer.
            retryCount++;
            if (retryCount > maxRetries) {
                logMessage("[Modbus] Read fail (Addr: " + String(startAddress + pointsRead) + ")");
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}
