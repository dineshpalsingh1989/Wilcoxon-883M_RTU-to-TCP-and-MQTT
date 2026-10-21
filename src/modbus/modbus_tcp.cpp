#include "core/modules.h"

// PLC-visible holding registers. Waveform and spectrum are copies taken when a
// capture completes, so a PLC never reads a buffer that is still downloading.
namespace {
uint16_t liveRegisters[100];
CAPTURE_BUFFER(uint16_t, waveformRegisters, WAVEFORM_TOTAL_POINTS);
CAPTURE_BUFFER(uint16_t, spectrumRegisters, SPECTRUM_TOTAL_POINTS);
Wilcoxon883MTCP::RegisterBlocks<3> plcRegisters;
Wilcoxon883MTCP plcServer(plcRegisters);

bool lockData(void*) { return xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE; }
void unlockData(void*) { xSemaphoreGive(dataMutex); }
}

bool allocateModbusTCPBuffers() {
#if CONFIG_IDF_TARGET_ESP32
    return allocateBuffer(waveformRegisters, WAVEFORM_TOTAL_POINTS) && allocateBuffer(spectrumRegisters, SPECTRUM_TOTAL_POINTS);
#else
    return true;
#endif
}

bool beginModbusTCPServer(uint16_t port) {
    // 0-99 stay writable as before; values are overwritten by the next sensor update.
    bool mapped = plcRegisters.addHolding(0, liveRegisters, 100, true) &&
                  plcRegisters.addHolding(HREG_WAVEFORM_START, waveformRegisters, WAVEFORM_TOTAL_POINTS) &&
                  plcRegisters.addHolding(HREG_SPECTRUM_START, spectrumRegisters, SPECTRUM_TOTAL_POINTS);
    plcRegisters.setLock(lockData, unlockData);
    // Same capacity and unit-ID handling as the previous server: 8 clients, any unit ID.
    return mapped && plcServer.begin(port, Wilcoxon883MTCP::MaxClients, 0);
}

void pollModbusTCPServer() {
    if (modbusTcpEnabled) plcServer.poll();
}

void setModbusTCPRegister(uint16_t address, uint16_t value) {
    if (address < 100) liveRegisters[address] = value;
}

// =================================================================
// --- MODBUS TCP HELPERS (LINEAR MAPPING)
// =================================================================
void updateModbusTCPCache() {
    if (!modbusTcpEnabled) return;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // X
        liveRegisters[0] = uint16_t((int16_t)(accelRmsX * 100));
        liveRegisters[1] = uint16_t((int16_t)(accelPeakX * 100));
        liveRegisters[2] = uint16_t((int16_t)(accelRms2X * 100));
        liveRegisters[3] = uint16_t((int16_t)(accelPeak2X * 100));
        liveRegisters[4] = uint16_t((int16_t)(veloRmsX * 10));
        liveRegisters[5] = uint16_t((int16_t)(veloPeakX * 10));
        liveRegisters[6] = uint16_t((int16_t)(displRmsX * 1));
        liveRegisters[7] = uint16_t((int16_t)(displPeakX * 1));
        liveRegisters[8] = uint16_t((int16_t)(truePeakX * 100));
        liveRegisters[9] = uint16_t((int16_t)(crestFactorX * 100));
        liveRegisters[10] = uint16_t((int16_t)(accelMetricRmsX * 10));
        liveRegisters[11] = uint16_t((int16_t)(accelMetricPeakX * 10));
        liveRegisters[12] = uint16_t((int16_t)(accelMetricRms2X * 10));
        liveRegisters[13] = uint16_t((int16_t)(accelMetricPeak2X * 10));
        liveRegisters[14] = uint16_t((int16_t)(veloWideRmsX * 10));
        liveRegisters[15] = uint16_t((int16_t)(veloWidePeakX * 10));
        liveRegisters[16] = uint16_t((int16_t)(displLowRmsX * 1));
        liveRegisters[17] = uint16_t((int16_t)(displLowPeakX * 1));
        // Y
        liveRegisters[20] = uint16_t((int16_t)(accelRmsY * 100));
        liveRegisters[21] = uint16_t((int16_t)(accelPeakY * 100));
        liveRegisters[22] = uint16_t((int16_t)(accelRms2Y * 100));
        liveRegisters[23] = uint16_t((int16_t)(accelPeak2Y * 100));
        liveRegisters[24] = uint16_t((int16_t)(veloRmsY * 10));
        liveRegisters[25] = uint16_t((int16_t)(veloPeakY * 10));
        liveRegisters[26] = uint16_t((int16_t)(displRmsY * 1));
        liveRegisters[27] = uint16_t((int16_t)(displPeakY * 1));
        liveRegisters[28] = uint16_t((int16_t)(truePeakY * 100));
        liveRegisters[29] = uint16_t((int16_t)(crestFactorY * 100));
        liveRegisters[30] = uint16_t((int16_t)(accelMetricRmsY * 10));
        liveRegisters[31] = uint16_t((int16_t)(accelMetricPeakY * 10));
        liveRegisters[32] = uint16_t((int16_t)(accelMetricRms2Y * 10));
        liveRegisters[33] = uint16_t((int16_t)(accelMetricPeak2Y * 10));
        liveRegisters[34] = uint16_t((int16_t)(veloWideRmsY * 10));
        liveRegisters[35] = uint16_t((int16_t)(veloWidePeakY * 10));
        liveRegisters[36] = uint16_t((int16_t)(displLowRmsY * 1));
        liveRegisters[37] = uint16_t((int16_t)(displLowPeakY * 1));
        // Z
        liveRegisters[40] = uint16_t((int16_t)(accelRmsZ * 100));
        liveRegisters[41] = uint16_t((int16_t)(accelPeakZ * 100));
        liveRegisters[42] = uint16_t((int16_t)(accelRms2Z * 100));
        liveRegisters[43] = uint16_t((int16_t)(accelPeak2Z * 100));
        liveRegisters[44] = uint16_t((int16_t)(veloRmsZ * 10));
        liveRegisters[45] = uint16_t((int16_t)(veloPeakZ * 10));
        liveRegisters[46] = uint16_t((int16_t)(displRmsZ * 1));
        liveRegisters[47] = uint16_t((int16_t)(displPeakZ * 1));
        liveRegisters[48] = uint16_t((int16_t)(truePeakZ * 100));
        liveRegisters[49] = uint16_t((int16_t)(crestFactorZ * 100));
        liveRegisters[50] = uint16_t((int16_t)(accelMetricRmsZ * 10));
        liveRegisters[51] = uint16_t((int16_t)(accelMetricPeakZ * 10));
        liveRegisters[52] = uint16_t((int16_t)(accelMetricRms2Z * 10));
        liveRegisters[53] = uint16_t((int16_t)(accelMetricPeak2Z * 10));
        liveRegisters[54] = uint16_t((int16_t)(veloWideRmsZ * 10));
        liveRegisters[55] = uint16_t((int16_t)(veloWidePeakZ * 10));
        liveRegisters[56] = uint16_t((int16_t)(displLowRmsZ * 1));
        liveRegisters[57] = uint16_t((int16_t)(displLowPeakZ * 1));
        // Other
        liveRegisters[60] = uint16_t((int16_t)(temperature * 10));

        xSemaphoreGive(dataMutex);
    }
}

// --- COPY FULL BUFFERS TO MODBUS REGISTERS ---
void syncModbusRegisters() {
    if (!modbusTcpEnabled) return;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        // int16_t samples are served as their raw 16-bit words, as before.
        if (newWaveformAvailable) memcpy(waveformRegisters, fullWaveform, WAVEFORM_TOTAL_POINTS * sizeof(uint16_t));
        if (newSpectrumAvailable) memcpy(spectrumRegisters, spectrumData, SPECTRUM_TOTAL_POINTS * sizeof(uint16_t));

        xSemaphoreGive(dataMutex);
    }
}
