#include "Wilcoxon883MRTU.h"
#include <driver/gpio.h>

const uint16_t Wilcoxon883MRTU::MaxRegisters;

Wilcoxon883MRTU::Wilcoxon883MRTU(HardwareSerial& serial) : transport_(serial), client_(transport_) {}

bool Wilcoxon883MRTU::begin(int rx, int tx, int direction, uint32_t baud, uint8_t unit, uint32_t format) {
    if (rx < 0 || tx < 0 || direction < 0 || !GPIO_IS_VALID_GPIO(rx) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(tx) || !GPIO_IS_VALID_OUTPUT_GPIO(direction) ||
        rx == tx || rx == direction || tx == direction || unit < 1 || unit > 247) return false;
    switch (baud) {
        case 1200: case 2400: case 4800: case 9600: case 19200: case 38400:
        case 57600: case 115200: case 230400: case 460800: case 921600: break;
        default: return false;
    }
    uint8_t characterBits;
    switch (format) {
        case SERIAL_8N1: characterBits = 10; break;
        case SERIAL_8N2: case SERIAL_8E1: case SERIAL_8O1: characterBits = 11; break;
        default: return false;
    }
    client_.reset();
    transport_.direction = direction;
    pinMode(direction, OUTPUT); transport_.tx(false);
    transport_.serial.begin(baud, format, rx, tx);
    if (!transport_.serial) return false;
    return client_.configure(baud, unit, characterBits);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::readInputRegisters(uint16_t address, uint16_t count,
        uint16_t* values, size_t capacity, uint32_t timeout) {
    return client_.readInput(address, count, values, capacity, timeout);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::readHoldingRegisters(uint16_t address, uint16_t count,
        uint16_t* values, size_t capacity, uint32_t timeout) {
    return client_.readHolding(address, count, values, capacity, timeout);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::writeSingleRegister(uint16_t address, uint16_t value, uint32_t timeout) {
    return client_.writeSingle(address, value, timeout);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::readMetrics(Metrics& metrics, uint32_t timeout) {
    return wilcoxon883m::readMetrics(client_, metrics, timeout);
}
const char* Wilcoxon883MRTU::resultText(Result value) {
    switch (value) {
        case Result::Ok: return "OK";
        case Result::InvalidArgument: return "Invalid argument or UART not initialized";
        case Result::Timeout: return "Response timeout";
        case Result::Frame: return "Invalid response frame";
        case Result::Crc: return "CRC mismatch";
        case Result::Exception: return "Sensor Modbus exception";
        case Result::Transport: return "UART write failed";
    }
    return "Unknown result";
}
