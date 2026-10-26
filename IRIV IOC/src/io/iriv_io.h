#pragma once
#include "core/globals.h"

#if GATEWAY_ETHERNET
// IRIV IO Controller I/O for the 883M gateway (pins from the IRIV datasheet, via the board variant):
//   DO0-DO3    solid-state relays; each one is assigned a source on the IOC I/O page:
//              dashboard alarm, sensor fault, an 883M value (axis + metric + threshold) or AN0/AN1
//              (threshold). A source can drive only one output. Default: DO0 alarm, DO1 fault.
//   DI0 (GP0)  rising edge starts an XYZ capture
//   DI1 (GP1)  rising edge acknowledges latched outputs (same as a short USER button press)
//   DI2-DI10   reported only
//   AN0/AN1    0-10 V or 4-20 mA (set by the DIP switch under the IOC), scaled to a process value
// GPIO and ADC run in a small sampler task. Rules, captures, logging, MQTT and Modbus run in loop().
namespace io {
constexpr int DigitalCount = 11, AnalogCount = 2, OutputCount = 4;
enum AnalogMode : uint8_t { AnalogOff = 0, AnalogCurrent = 1, AnalogVoltage = 2 };
enum OutputSource : uint8_t { SourceOff = 0, SourceAlarm = 1, SourceFault = 2, SourceMetric = 3, SourceAn0 = 4, SourceAn1 = 5 };
enum Axis : uint8_t { AxisX = 0, AxisY = 1, AxisZ = 2, AxisAny = 3 }; // Any = highest of X, Y and Z
enum Direction : uint8_t { Above = 0, Below = 1 };

// 883M values a relay can follow. Units and ranges are from the 883M manual's measurement table.
struct MetricInfo { const char* id; const char* label; const char* unit; bool perAxis; };
constexpr int MetricCount = 14;
extern const MetricInfo Metrics[MetricCount];

struct AnalogConfig {
    char name[25] = "";
    char unit[13] = "";
    uint8_t mode = AnalogOff;
    float minimum = 0, maximum = 100; // engineering value at 4 mA / 0 V and at 20 mA / 10 V
};

struct OutputConfig {
    uint8_t source = SourceOff, metric = 0, axis = AxisAny, direction = Above;
    float threshold = 0, hysteresis = 0;
    uint16_t delaySeconds = 0;
    bool latch = false; // stay on after the condition clears, until acknowledged
};

struct Snapshot {
    bool ready = false;
    uint16_t inputs = 0;  // bit n = DIn active
    uint8_t outputs = 0;  // bit n = DOn on
    uint8_t conditions = 0, latched = 0, acknowledged = 0; // per output
    bool alarm = false, sensorFault = false;
    uint16_t adc[AnalogCount] = {};
    float measured[AnalogCount] = {}; // mA or V at the terminal
    float value[AnalogCount] = {};    // scaled engineering value
    bool analogFault[AnalogCount] = {};
    AnalogConfig analog[AnalogCount];
    OutputConfig output[OutputCount];
    uint32_t sequence = 0, revision = 0, captureAccepted = 0, captureRejected = 0;
};

struct Requests { bool capture = false, acknowledge = false; };

bool begin();
// Called from loop(): hands over each output's condition plus the alarm/fault state for reporting,
// and returns DI0/DI1 edges since the last call.
Requests service(uint8_t conditions, bool alarmActive, bool sensorFault);
void acknowledge();                       // USER button, DI1 or the web page
void recordCapture(bool accepted);        // result of a DI0 request, for the status page
bool snapshot(Snapshot& out);
bool setAnalogConfig(const AnalogConfig (&analog)[AnalogCount]);   // stores and applies
bool setOutputConfig(const OutputConfig (&output)[OutputCount]);   // stores and applies
// Returns an error text, or nullptr when the outputs are valid (including one output per source).
const char* validateOutputs(const OutputConfig (&output)[OutputCount]);
// formFields adds the numeric rule settings the IOC I/O page edits.
void writeJson(JsonObject out, const Snapshot& state, bool formFields = false);
// Holding registers 70-79 (see README); returns false when no sample is available yet.
bool fillModbusRegisters(uint16_t (&registers)[10]);
// Current value of an 883M metric; caller holds dataMutex. Defined in io_rules.cpp.
float metricValue(uint8_t metric, uint8_t axis);
}
#endif
