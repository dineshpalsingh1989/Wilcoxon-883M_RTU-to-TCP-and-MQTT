#pragma once
#include "core/globals.h"

void initStatusLed();
void updateLedStatus();
// MAX485 DE/RE (RS485_CTRL_PIN) is driven by Wilcoxon883MRTU around each request.
