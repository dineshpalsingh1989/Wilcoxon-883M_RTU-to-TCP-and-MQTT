#pragma once
#include "core/globals.h"

void publishDeviceHealth();
void publishMetrics();
void publishDownsampledWaveform();
// Whole waveform / spectrum of one axis as one MQTT message; false if it was not fully sent.
bool publishFullWaveform(Axis capturedAxis);
bool publishFullSpectrum(Axis capturedAxis);
void publishSpectrumPeaks(Axis capturedAxis, const std::vector<SpectrumPeak>& peaks);
void publishCombinedSpectrumPeaks();
void mqttCallback(char* topic, byte* payload, unsigned int length);
