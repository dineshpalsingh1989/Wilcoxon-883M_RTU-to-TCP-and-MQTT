#pragma once
#include "core/globals.h"

void updateLedStatus();
void preTransmission();   // MAX485: DE/RE high (transmit)
void postTransmission();  // MAX485: DE/RE low (receive)
