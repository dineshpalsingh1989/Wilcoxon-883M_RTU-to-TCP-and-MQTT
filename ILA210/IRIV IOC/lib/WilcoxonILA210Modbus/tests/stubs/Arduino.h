#pragma once
#include <stdint.h>
#include <stddef.h>
uint32_t millis();
uint32_t micros();
void yield();
void delayMicroseconds(uint32_t);
