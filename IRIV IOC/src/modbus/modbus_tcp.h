#pragma once
#include "core/globals.h"

// Call once at boot, before any capture or PLC access.
bool allocateModbusTCPBuffers();
bool beginModbusTCPServer(uint16_t port);
// Background task only; answers PLC requests without blocking.
void pollModbusTCPServer();
// Sets a holding register in 0-99. Caller holds dataMutex.
void setModbusTCPRegister(uint16_t address, uint16_t value);
void updateModbusTCPCache();
void syncModbusRegisters();
#if GATEWAY_ETHERNET
// Copies the IRIV I/O state into holding registers 70-79.
void syncIoModbusRegisters();
#endif
// Server state and counters from the Wilcoxon883MTCP library, for the Libraries page.
void addModbusTcpLibraryStatus(JsonObject out);
