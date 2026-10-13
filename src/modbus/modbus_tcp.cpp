#include "core/modules.h"

// =================================================================
// --- MODBUS TCP HELPERS (LINEAR MAPPING)
// =================================================================
void updateModbusTCPCache() {
    if (!modbusTcpEnabled) return;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // X
        modbusTCPServer.Hreg(0, (int16_t)(accelRmsX * 100));
        modbusTCPServer.Hreg(1, (int16_t)(accelPeakX * 100));
        modbusTCPServer.Hreg(2, (int16_t)(accelRms2X * 100));
        modbusTCPServer.Hreg(3, (int16_t)(accelPeak2X * 100));
        modbusTCPServer.Hreg(4, (int16_t)(veloRmsX * 10));
        modbusTCPServer.Hreg(5, (int16_t)(veloPeakX * 10));
        modbusTCPServer.Hreg(6, (int16_t)(displRmsX * 1));
        modbusTCPServer.Hreg(7, (int16_t)(displPeakX * 1));
        modbusTCPServer.Hreg(8, (int16_t)(truePeakX * 100));
        modbusTCPServer.Hreg(9, (int16_t)(crestFactorX * 100));
        modbusTCPServer.Hreg(10, (int16_t)(accelMetricRmsX * 10));
        modbusTCPServer.Hreg(11, (int16_t)(accelMetricPeakX * 10));
        modbusTCPServer.Hreg(12, (int16_t)(accelMetricRms2X * 10));
        modbusTCPServer.Hreg(13, (int16_t)(accelMetricPeak2X * 10));
        modbusTCPServer.Hreg(14, (int16_t)(veloWideRmsX * 10));
        modbusTCPServer.Hreg(15, (int16_t)(veloWidePeakX * 10));
        modbusTCPServer.Hreg(16, (int16_t)(displLowRmsX * 1));
        modbusTCPServer.Hreg(17, (int16_t)(displLowPeakX * 1));
        // Y
        modbusTCPServer.Hreg(20, (int16_t)(accelRmsY * 100));
        modbusTCPServer.Hreg(21, (int16_t)(accelPeakY * 100));
        modbusTCPServer.Hreg(22, (int16_t)(accelRms2Y * 100));
        modbusTCPServer.Hreg(23, (int16_t)(accelPeak2Y * 100));
        modbusTCPServer.Hreg(24, (int16_t)(veloRmsY * 10));
        modbusTCPServer.Hreg(25, (int16_t)(veloPeakY * 10));
        modbusTCPServer.Hreg(26, (int16_t)(displRmsY * 1));
        modbusTCPServer.Hreg(27, (int16_t)(displPeakY * 1));
        modbusTCPServer.Hreg(28, (int16_t)(truePeakY * 100));
        modbusTCPServer.Hreg(29, (int16_t)(crestFactorY * 100));
        modbusTCPServer.Hreg(30, (int16_t)(accelMetricRmsY * 10));
        modbusTCPServer.Hreg(31, (int16_t)(accelMetricPeakY * 10));
        modbusTCPServer.Hreg(32, (int16_t)(accelMetricRms2Y * 10));
        modbusTCPServer.Hreg(33, (int16_t)(accelMetricPeak2Y * 10));
        modbusTCPServer.Hreg(34, (int16_t)(veloWideRmsY * 10));
        modbusTCPServer.Hreg(35, (int16_t)(veloWidePeakY * 10));
        modbusTCPServer.Hreg(36, (int16_t)(displLowRmsY * 1));
        modbusTCPServer.Hreg(37, (int16_t)(displLowPeakY * 1));
        // Z
        modbusTCPServer.Hreg(40, (int16_t)(accelRmsZ * 100));
        modbusTCPServer.Hreg(41, (int16_t)(accelPeakZ * 100));
        modbusTCPServer.Hreg(42, (int16_t)(accelRms2Z * 100));
        modbusTCPServer.Hreg(43, (int16_t)(accelPeak2Z * 100));
        modbusTCPServer.Hreg(44, (int16_t)(veloRmsZ * 10));
        modbusTCPServer.Hreg(45, (int16_t)(veloPeakZ * 10));
        modbusTCPServer.Hreg(46, (int16_t)(displRmsZ * 1));
        modbusTCPServer.Hreg(47, (int16_t)(displPeakZ * 1));
        modbusTCPServer.Hreg(48, (int16_t)(truePeakZ * 100));
        modbusTCPServer.Hreg(49, (int16_t)(crestFactorZ * 100));
        modbusTCPServer.Hreg(50, (int16_t)(accelMetricRmsZ * 10));
        modbusTCPServer.Hreg(51, (int16_t)(accelMetricPeakZ * 10));
        modbusTCPServer.Hreg(52, (int16_t)(accelMetricRms2Z * 10));
        modbusTCPServer.Hreg(53, (int16_t)(accelMetricPeak2Z * 10));
        modbusTCPServer.Hreg(54, (int16_t)(veloWideRmsZ * 10));
        modbusTCPServer.Hreg(55, (int16_t)(veloWidePeakZ * 10));
        modbusTCPServer.Hreg(56, (int16_t)(displLowRmsZ * 1));
        modbusTCPServer.Hreg(57, (int16_t)(displLowPeakZ * 1));
        // Other
        modbusTCPServer.Hreg(60, (int16_t)(temperature * 10));

        xSemaphoreGive(dataMutex);
    }
}

// --- COPY FULL BUFFERS TO MODBUS REGISTERS ---
void syncModbusRegisters() {
    if (!modbusTcpEnabled) return;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        if (newWaveformAvailable) {
            for (int i = 0; i < WAVEFORM_TOTAL_POINTS; i++) {
                modbusTCPServer.Hreg(HREG_WAVEFORM_START + i, fullWaveform[i]);
            }
        }

        if (newSpectrumAvailable) {
            for (int i = 0; i < SPECTRUM_TOTAL_POINTS; i++) {
                modbusTCPServer.Hreg(HREG_SPECTRUM_START + i, spectrumData[i]);
            }
        }

        xSemaphoreGive(dataMutex);
    }
}
