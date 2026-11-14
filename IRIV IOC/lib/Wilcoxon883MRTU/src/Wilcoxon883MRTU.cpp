#include "Wilcoxon883MRTU.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <driver/gpio.h>
#endif

const uint16_t Wilcoxon883MRTU::MaxRegisters;

Wilcoxon883MRTU::Wilcoxon883MRTU(Uart& serial) : transport_(serial), client_(transport_), probe_(*this), discovery_(probe_) {}

namespace {
// direction -1 means the transceiver switches direction itself.
bool validPins(int rx,int tx,int direction) {
#if defined(ARDUINO_ARCH_ESP32)
    const bool pinsExist = GPIO_IS_VALID_GPIO(rx) && GPIO_IS_VALID_OUTPUT_GPIO(tx) &&
        (direction < 0 || GPIO_IS_VALID_OUTPUT_GPIO(direction));
#else
    const bool pinsExist = rx < 48 && tx < 48 && direction < 48;
#endif
    return rx>=0 && tx>=0 && direction>=-1 && pinsExist &&
        rx!=tx && rx!=direction && tx!=direction;
}
uint32_t serialFormat(Wilcoxon883MRTU::SerialMode mode) {
    switch(mode) {
        case Wilcoxon883MRTU::SerialMode::N81:return SERIAL_8N1;
        case Wilcoxon883MRTU::SerialMode::N82:return SERIAL_8N2;
        case Wilcoxon883MRTU::SerialMode::E81:return SERIAL_8E1;
        case Wilcoxon883MRTU::SerialMode::O81:return SERIAL_8O1;
        case Wilcoxon883MRTU::SerialMode::E82:return SERIAL_8E2;
        case Wilcoxon883MRTU::SerialMode::O82:return SERIAL_8O2;
        default:return 0;
    }
}
}
bool Wilcoxon883MRTU::begin(int rx,int tx,int direction,uint32_t baud,uint8_t unit,uint32_t format) {
    if(discovering())return false;
    return configureUart(rx,tx,direction,baud,unit,format);
}
bool Wilcoxon883MRTU::configureUart(int rx, int tx, int direction, uint32_t baud, uint8_t unit, uint32_t format) {
    if(!validPins(rx,tx,direction) || unit<1 || unit>247)return false;
    switch (baud) {
        case 1200: case 2400: case 4800: case 9600: case 19200: case 38400:
        case 57600: case 115200: case 230400: case 460800: case 921600: break;
        default: return false;
    }
    uint8_t characterBits;
    switch (format) {
        case SERIAL_8N1: characterBits = 10; break;
        case SERIAL_8N2: case SERIAL_8E1: case SERIAL_8O1: characterBits = 11; break;
        case SERIAL_8E2: case SERIAL_8O2: characterBits = 12; break;
        default: return false;
    }
    client_.reset();
    transport_.direction = direction;
    if (direction >= 0) pinMode(direction, OUTPUT);
    transport_.tx(false);
#if defined(ARDUINO_ARCH_RP2040)
    transport_.serial.end();
    if (!transport_.serial.setRX(rx) || !transport_.serial.setTX(tx)) return false;
    transport_.serial.begin(baud, static_cast<uint16_t>(format));
#else
    transport_.serial.begin(baud, format, rx, tx);
#endif
    if (!transport_.serial) return false;
    return client_.configure(baud, unit, characterBits);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::readInputRegisters(uint16_t address, uint16_t count,
        uint16_t* values, size_t capacity, uint32_t timeout) {
    if(discovering())return Result::Busy;
    return client_.readInput(address, count, values, capacity, timeout);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::readHoldingRegisters(uint16_t address, uint16_t count,
        uint16_t* values, size_t capacity, uint32_t timeout) {
    if(discovering())return Result::Busy;
    return client_.readHolding(address, count, values, capacity, timeout);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::writeSingleRegister(uint16_t address, uint16_t value, uint32_t timeout) {
    if(discovering())return Result::Busy;
    return client_.writeSingle(address, value, timeout);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::readMetrics(Metrics& metrics, uint32_t timeout) {
    if(discovering())return Result::Busy;
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
        case Result::Busy: return "Discovery owns the UART; finish or cancel it first";
    }
    return "Unknown result";
}

bool Wilcoxon883MRTU::DiscoveryProbe::configure(const wilcoxon883m::Settings& settings) {
    return owner.configureUart(owner.discoveryRx_,owner.discoveryTx_,owner.discoveryDirection_,
        settings.baud,settings.unit,serialFormat(settings.mode));
}
bool Wilcoxon883MRTU::startDiscovery(int rx,int tx,int direction,const DiscoveryOptions& options) {
    if(!validPins(rx,tx,direction) || !wilcoxon883m::Discovery<DiscoveryProbe>::valid(options))return false;
    discoveryRx_=rx;discoveryTx_=tx;discoveryDirection_=direction;
    return discovery_.start(options);
}
Wilcoxon883MRTU::Result Wilcoxon883MRTU::readIdentity(Identity& identity,uint32_t timeout) {
    if(discovering())return Result::Busy;
    return client_.readIdentity(identity,timeout);
}
const char* Wilcoxon883MRTU::serialModeText(SerialMode mode) {
    switch(mode){
        case SerialMode::N81:return "8N1";case SerialMode::N82:return "8N2";
        case SerialMode::E81:return "8E1";case SerialMode::O81:return "8O1";
        case SerialMode::E82:return "8E2";case SerialMode::O82:return "8O2";
        default:return "Any";
    }
}

const char* Wilcoxon883MRTU::discoveryStateText(DiscoveryState state) {
    switch(state){
        case DiscoveryState::Idle:return "Not started";
        case DiscoveryState::Searching:return "Searching";
        case DiscoveryState::Found:return "Found";
        case DiscoveryState::Exhausted:return "All candidates tried";
        case DiscoveryState::TimedOut:return "Time limit reached (search may be incomplete)";
        case DiscoveryState::Cancelled:return "Cancelled";
    }
    return "Unknown state";
}
