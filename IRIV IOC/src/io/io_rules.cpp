#include "core/modules.h"

#if GATEWAY_ETHERNET
#include <algorithm>

namespace io {
// Order is the stored metric index; append only. Only values the gateway reads from the sensor.
const MetricInfo Metrics[MetricCount] = {
    {"velo_rms", "Velocity RMS (10 Hz-1 kHz)", "mm/s", true},
    {"velo_peak", "Velocity peak (10 Hz-1 kHz)", "mm/s", true},
    {"velo_rms_hf", "Velocity RMS (10 Hz-5 kHz)", "mm/s", true},
    {"velo_peak_hf", "Velocity peak (10 Hz-5 kHz)", "mm/s", true},
    {"accel_rms", "Acceleration RMS (2 Hz-1 kHz)", "g", true},
    {"accel_peak", "Acceleration peak (2 Hz-1 kHz)", "g", true},
    {"accel_rms_hf", "Acceleration RMS (10 Hz-5 kHz)", "g", true},
    {"accel_peak_hf", "Acceleration peak (10 Hz-5 kHz)", "g", true},
    {"true_peak", "True peak", "g", true},
    {"displ_rms", "Displacement RMS (2 Hz-1 kHz)", "um", true},
    {"displ_peak", "Displacement peak (2 Hz-1 kHz)", "um", true},
    {"crest_factor", "Crest factor", "", true},
    {"std_dev", "Standard deviation", "", true},
    {"temperature", "Temperature", "C", false},
};

float metricValue(uint8_t metric, uint8_t axis) {
    auto pick = [axis](float x, float y, float z) {
        return axis == AxisX ? x : axis == AxisY ? y : axis == AxisZ ? z : std::max(x, std::max(y, z));
    };
    switch (metric) {
        case 0: return pick(veloRmsX, veloRmsY, veloRmsZ);
        case 1: return pick(veloPeakX, veloPeakY, veloPeakZ);
        case 2: return pick(veloWideRmsX, veloWideRmsY, veloWideRmsZ);
        case 3: return pick(veloWidePeakX, veloWidePeakY, veloWidePeakZ);
        case 4: return pick(accelRmsX, accelRmsY, accelRmsZ);
        case 5: return pick(accelPeakX, accelPeakY, accelPeakZ);
        case 6: return pick(accelRms2X, accelRms2Y, accelRms2Z);
        case 7: return pick(accelPeak2X, accelPeak2Y, accelPeak2Z);
        case 8: return pick(truePeakX, truePeakY, truePeakZ);
        case 9: return pick(displLowRmsX, displLowRmsY, displLowRmsZ);
        case 10: return pick(displLowPeakX, displLowPeakY, displLowPeakZ);
        case 11: return pick(crestFactorX, crestFactorY, crestFactorZ);
        case 12: return pick(stdDeviationX, stdDeviationY, stdDeviationZ);
        case 13: return temperature;
        default: return NAN;
    }
}
}
#endif
