#pragma once
#include <stdint.h>
#include "config/config.h"

enum DynamicDataState { DD_IDLE, DD_REQUEST_DATA, DD_WAITING_FOR_DATA };
enum CaptureMode { CAPTURE_NONE, CAPTURE_WAVEFORM, CAPTURE_SPECTRUM, CAPTURE_BOTH };
enum Axis { AXIS_NONE, AXIS_X, AXIS_Y, AXIS_Z };
struct SpectrumPeak { float frequency_hz; float amplitude_g; };
struct MetricsSnapshot { char timestamp[21] = {0}; float values[METRICS_SNAPSHOT_SIZE]; bool is_valid = false; };
