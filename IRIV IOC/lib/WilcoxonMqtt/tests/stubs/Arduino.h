#pragma once
// Host stand-ins for the few Arduino calls WilcoxonMqtt uses. The protocol tests use a
// fake clock that only moves in delay(); the live broker test uses the real clock.
#include <stddef.h>
#include <stdint.h>
#ifdef WILCOXON_MQTT_LIVE
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
inline uint32_t millis() { return GetTickCount(); }
inline void delay(uint32_t ms) { Sleep(ms); }
#else
#include <time.h>
inline uint32_t millis() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return uint32_t(t.tv_sec * 1000 + t.tv_nsec / 1000000); }
inline void delay(uint32_t ms) { timespec t = {time_t(ms / 1000), long(ms % 1000) * 1000000L}; nanosleep(&t, nullptr); }
#endif
#else
extern uint32_t fakeNow;
inline uint32_t millis() { return fakeNow; }
inline void delay(uint32_t ms) { fakeNow += ms; }
#endif
