#pragma once
#include "core/globals.h"

void webServerStep();
void backgroundStep();
void webServerLoop(void * pvParameters);
void backgroundLoop(void * pvParameters);
// Keeps the web pages answering during long sensor downloads on the IRIV, where web and
// sensor work share one task. Nothing to do on ESP32 boards, which use separate tasks.
void serviceWebDuringLongWork();
