#include "core/modules.h"

// =================================================================
// --- MODBUS IDENTITY FUNCTION
// =================================================================
void fetchSensorIdentity() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    
    logMessage("[System] Fetching Sensor Identity...");

    // Must enable TX manually for raw serial writes
    digitalWrite(RS485_CTRL_PIN, HIGH);
    delay(2);

    uint8_t readCodes[2] = {0x01, 0x02};
    for (int q = 0; q < 2; q++) {
        uint8_t currentCode = readCodes[q];
        while(Serial1.available()) Serial1.read();
        
        uint8_t request[] = {modbus_slave_id_val, 0x2B, 0x0E, currentCode, 0x00, 0x00, 0x00};
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < 5; i++) {
            crc ^= request[i];
            for (int j = 0; j < 8; j++) { if (crc & 1) crc = (crc >> 1) ^ 0xA001; else crc >>= 1; }
        }
        request[5] = crc & 0xFF; request[6] = crc >> 8;

        Serial1.write(request, 7);
        Serial1.flush(); // Wait for TX to finish
        
        // Switch to RX immediately after flush
        digitalWrite(RS485_CTRL_PIN, LOW); 
        delay(300); 

        uint8_t buffer[256]; 
        int idx = 0;
        while(Serial1.available() && idx < 256) { buffer[idx++] = Serial1.read(); }

        if (idx > 8 && buffer[1] == 0x2B && buffer[2] == 0x0E && buffer[3] == currentCode) {
            int objCount = buffer[7]; int ptr = 8; 
            for (int i = 0; i < objCount; i++) {
                if (ptr + 2 >= idx) break; 
                uint8_t objId = buffer[ptr]; uint8_t objLen = buffer[ptr+1];
                String objData = "";
                for (int k = 0; k < objLen; k++) { if (ptr + 2 + k < idx) objData += (char)buffer[ptr + 2 + k]; }
                if (objId == 0x01) sensor_serial = objData;        
                if (objId == 0x02) sensor_fw_version = objData;  
                if (objId == 0x05) sensor_model = objData;        
                if (objId == 0x03) sensor_vendor_url = objData;  
                if (objId == 0x04) sensor_part_number = objData; 
                ptr += (2 + objLen); 
            }
        }
        delay(100); 
        // Re-enable TX for next loop iteration
        digitalWrite(RS485_CTRL_PIN, HIGH);
    }
    
    // Ensure we end in RX mode
    digitalWrite(RS485_CTRL_PIN, LOW);

    logMessage("[System] ID Result -> Model: " + sensor_model + " | Serial: " + sensor_serial + " | PN: " + sensor_part_number);
    xSemaphoreGive(dataMutex);
}

bool requestNewWaveform() {
    uint8_t result = node.writeSingleRegister(REG_TRIGGER, VAL_TRIGGER_START);
    return (result == node.ku8MBSuccess);
}

bool isWaveformReady() {
    uint8_t result = node.readHoldingRegisters(REG_TRIGGER, 1);
    if (result == node.ku8MBSuccess) {
        return (node.getResponseBuffer(0) == VAL_STATUS_READY);
    }
    return false;
}

bool readSensorValue(const char* name, uint16_t address, float scale, float &target) {
    uint8_t result = node.readInputRegisters(address, 1);
    if (result == node.ku8MBSuccess) {
        target = (int16_t)node.getResponseBuffer(0) / scale;
        return true;
    }
    return false;
}

bool readLargeDataBlock(uint16_t startAddress, uint16_t totalPoints, int16_t* buffer) {
    uint16_t pointsRead = 0;
    const uint16_t chunkSize = 125;
    int retryCount = 0;
    const int maxRetries = 3;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        snprintf(systemStatus, sizeof(systemStatus), "Downloading... (0%%)");
        strcpy(trafficLightStatus, "BUSY");
        xSemaphoreGive(dataMutex);
    }

    while (pointsRead < totalPoints) {
        esp_task_wdt_reset(); 

        if (modbusTcpEnabled) {
             modbusTCPServer.task();
        }

        uint16_t pointsLeft = totalPoints - pointsRead;
        uint16_t pointsToRead = (pointsLeft > chunkSize) ? chunkSize : pointsLeft;
        uint8_t result = node.readInputRegisters(startAddress + pointsRead, pointsToRead);

        if (result == node.ku8MBSuccess) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                for (int i = 0; i < pointsToRead; i++) {
                    buffer[pointsRead + i] = (int16_t)node.getResponseBuffer(i);
                }
                xSemaphoreGive(dataMutex);
            }
            
            pointsRead += pointsToRead;
            retryCount = 0;

            if (pointsRead % 1000 < chunkSize || pointsRead == totalPoints) {
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    snprintf(systemStatus, sizeof(systemStatus), "Downloading... (%d%%)", (pointsRead * 100) / totalPoints);
                    xSemaphoreGive(dataMutex);
                }
                if (mqttClient && mqttClient->connected()) mqttClient->loop(); 
                esp_task_wdt_reset(); 
            }
        } else {
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

