#pragma once
#include "core/globals.h"

void publishDeviceHealth();
void publishMetrics();
void publishDownsampledWaveform();
void publishFullWaveformInChunks(Axis capturedAxis);
void publishFullSpectrumInChunks(Axis capturedAxis);
void publishSpectrumPeaks(Axis capturedAxis, const std::vector<SpectrumPeak>& peaks);
void publishCombinedSpectrumPeaks();
void mqttCallback(char* topic, byte* payload, unsigned int length);
