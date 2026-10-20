#pragma once
#include "core/globals.h"

bool beginModbusTCPServer(uint16_t port);
// Background task only; answers PLC requests without blocking.
void pollModbusTCPServer();
// Sets a holding register in 0-99. Caller holds dataMutex.
void setModbusTCPRegister(uint16_t address, uint16_t value);
void updateModbusTCPCache();
void syncModbusRegisters();
