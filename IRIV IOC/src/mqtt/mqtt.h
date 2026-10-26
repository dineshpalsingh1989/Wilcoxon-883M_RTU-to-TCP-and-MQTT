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
#if GATEWAY_ETHERNET
// IRIV I/O state on <base topic>/io when it changes, and at least once a minute.
void publishIoStatus();
#endif
#if GATEWAY_ETHERNET
void setupSecureMqtt();
void serviceSecureMqtt();
bool mqttCertificateInstalled();
// State and last errors from the W5500SecureMqtt library, for the Libraries page.
void addSecureMqttLibraryStatus(JsonObject out);
// Connects now, skipping the retry delay; blocks for the TLS handshake. For the Libraries page tests.
bool connectSecureMqttNow();
void handleMqttCertificate();
#endif
