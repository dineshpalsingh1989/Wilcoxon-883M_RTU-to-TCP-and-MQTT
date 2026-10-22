#pragma once
#include "core/globals.h"

void webServerStep();
void backgroundStep();
void webServerLoop(void * pvParameters);
void backgroundLoop(void * pvParameters);
