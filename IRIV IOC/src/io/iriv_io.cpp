#include "core/modules.h"

#if GATEWAY_ETHERNET
#include <cmath>

namespace io {
namespace {
const uint8_t inputPins[DigitalCount] = {PIN_DI0, PIN_DI1, PIN_DI2, PIN_DI3, PIN_DI4, PIN_DI5,
                                         PIN_DI6, PIN_DI7, PIN_DI8, PIN_DI9, PIN_DI10};
const uint8_t outputPins[OutputCount] = {PIN_DO0, PIN_DO1, PIN_DO2, PIN_DO3};
const uint8_t analogPins[AnalogCount] = {PIN_AN0, PIN_AN1};
// Nominal RP2350 ADC reference; readings are not calibrated against a meter.
constexpr float AdcReferenceMv = 3300.0f;

// Same input stage as the ILA210 IRIV firmware: terminal = MCU pin * 16/5, 248 ohm in current mode.
float terminalVolts(uint16_t adc) { return adc / 4095.0f * AdcReferenceMv / 1000.0f * 16.0f / 5.0f; }
float terminalMilliamps(uint16_t adc) { return terminalVolts(adc) / 248.0f * 1000.0f; }
float engineeringValue(float input, bool current, float minimum, float maximum) {
    return minimum + (input - (current ? 4.0f : 0.0f)) / (current ? 16.0f : 10.0f) * (maximum - minimum);
}

struct Debounce {
    bool stable = false, candidate = false, initialized = false;
    uint32_t since = 0;
    // Returns true once on each debounced rising edge; a held input never repeats.
    bool update(bool value, uint32_t now) {
        if (!initialized) { stable = candidate = value; since = now; initialized = true; return false; }
        if (value != candidate) { candidate = value; since = now; }
        if (candidate != stable && uint32_t(now - since) >= 50) { stable = candidate; return stable; }
        return false;
    }
};

// A latched output stays on while its condition is true and afterwards until acknowledged.
// Acknowledging while the condition is still true lets it drop as soon as the condition clears.
struct Latch {
    bool latched = false, acknowledged = false, previous = false;
    void update(bool active, bool acknowledge) {
        if (active && !previous) acknowledged = false;
        if (active) latched = true;
        if (acknowledge) acknowledged = true;
        if (!active && acknowledged) latched = false;
        if (!latched) acknowledged = false;
        previous = active;
    }
};

SemaphoreHandle_t mutex = nullptr;
Snapshot state;
Latch latches[OutputCount];
uint8_t conditionInput = 0;
bool ackRequested = false, captureEdge = false, ackEdge = false;
StackType_t samplerStack[4096 / sizeof(StackType_t)];
StaticTask_t samplerTask;

void loadAnalog(AnalogConfig (&analog)[AnalogCount]) {
    Preferences store;
    store.begin("io-cfg", true);
    for (int i = 0; i < AnalogCount; ++i) {
        const String key = "an" + String(i) + "_";
        snprintf(analog[i].name, sizeof(analog[i].name), "%s", store.getString((key + "name").c_str(), "AN" + String(i)).c_str());
        snprintf(analog[i].unit, sizeof(analog[i].unit), "%s", store.getString((key + "unit").c_str(), "").c_str());
        analog[i].mode = store.getUChar((key + "mode").c_str(), AnalogOff);
        analog[i].model = store.getUChar((key + "model").c_str(), 0);
        analog[i].minimum = store.getFloat((key + "min").c_str(), 0);
        analog[i].maximum = store.getFloat((key + "max").c_str(), 100);
        if (analog[i].mode > AnalogVoltage || analog[i].model > 18 ||
            (analog[i].model && analog[i].mode != AnalogCurrent) ||
            !std::isfinite(analog[i].minimum) || !std::isfinite(analog[i].maximum) ||
            analog[i].minimum >= analog[i].maximum) {
            analog[i].mode = AnalogOff; analog[i].model = 0; analog[i].minimum = 0; analog[i].maximum = 100;
        }
    }
    store.end();
}

void loadInputs(InputConfig (&input)[DigitalCount]) {
    input[0].action = InputCapture;
    input[1].action = InputAcknowledge;
    Preferences store;
    store.begin("io-di", true);
    const uint32_t first = store.getUInt("di0_5", uint32_t(InputCapture) | (uint32_t(InputAcknowledge) << 5));
    const uint32_t second = store.getUInt("di6_10", 0);
    for (int i = 0; i < DigitalCount; ++i) {
        const uint32_t packed = i < 6 ? first >> (i * 5) : second >> ((i - 6) * 5);
        const uint8_t action = packed & 7;
        const uint8_t output = (packed >> 3) & 3;
        if (action <= InputAlarm && output < OutputCount) { input[i].action = action; input[i].output = output; }
    }
    store.end();
}

void loadOutputs(OutputConfig (&output)[OutputCount]) {
    // Defaults keep the original wiring: DO0 alarm (latched), DO1 sensor fault, DO2/DO3 off.
    output[0].source = SourceAlarm; output[0].latch = true;
    output[1].source = SourceFault;
    Preferences store;
    store.begin("io-out", true);
    OutputConfig loaded[OutputCount];
    for (int i = 0; i < OutputCount; ++i) {
        const String key = "do" + String(i) + "_";
        loaded[i].source = store.getUChar((key + "src").c_str(), output[i].source);
        loaded[i].metric = store.getUChar((key + "met").c_str(), 0);
        loaded[i].axis = store.getUChar((key + "axis").c_str(), AxisAny);
        loaded[i].direction = store.getUChar((key + "dir").c_str(), Above);
        loaded[i].threshold = store.getFloat((key + "thr").c_str(), 0);
        loaded[i].hysteresis = store.getFloat((key + "hys").c_str(), 0);
        loaded[i].delaySeconds = store.getUShort((key + "dly").c_str(), 0);
        loaded[i].latch = store.getBool((key + "lat").c_str(), output[i].latch);
    }
    store.end();
    if (!validateOutputs(loaded)) for (int i = 0; i < OutputCount; ++i) output[i] = loaded[i];
}

void sampler(void*) {
    Debounce inputs[DigitalCount];
    uint32_t analogAt = 0, revision = 0;
    for (;;) {
        const uint32_t now = millis();
        uint16_t mask = 0;
        uint16_t edges = 0;
        for (int i = 0; i < DigitalCount; ++i) {
            const bool rising = inputs[i].update(digitalRead(inputPins[i]) != LOW, now); // inputs are active high
            if (inputs[i].stable) mask |= 1u << i;
            if (rising) edges |= 1u << i;
        }
        AnalogConfig analog[AnalogCount];
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        for (int i = 0; i < AnalogCount; ++i) analog[i] = state.analog[i];
        xSemaphoreGive(mutex);

        uint16_t adc[AnalogCount] = {};
        float measured[AnalogCount] = {}, value[AnalogCount] = {};
        bool analogFault[AnalogCount] = {}, sampleAnalog = uint32_t(now - analogAt) >= 100;
        if (sampleAnalog) {
            analogAt = now;
            for (int i = 0; i < AnalogCount; ++i) {
                if (analog[i].mode == AnalogOff) continue;
                uint32_t total = 0;
                for (int n = 0; n < 8; ++n) total += analogRead(analogPins[i]);
                adc[i] = total / 8;
                const bool current = analog[i].mode == AnalogCurrent;
                measured[i] = current ? terminalMilliamps(adc[i]) : terminalVolts(adc[i]);
                value[i] = engineeringValue(measured[i], current, analog[i].minimum, analog[i].maximum);
                // Over-range, or a 4-20 mA loop outside 3.6-21 mA (wire break or failed transmitter).
                analogFault[i] = adc[i] >= 4090 || (current && (measured[i] < 3.6f || measured[i] > 21.0f));
            }
        }

        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (revision != state.revision) { for (auto& l : latches) l = Latch(); revision = state.revision; }
            bool acknowledgeInputNow = false;
            for (int i = 0; i < DigitalCount; ++i) {
                if (!(edges & (1u << i))) continue;
                if (state.input[i].action == InputCapture) captureEdge = true;
                if (state.input[i].action == InputAcknowledge) { ackEdge = true; acknowledgeInputNow = true; }
            }
            bool inputAlarm = false;
            uint8_t force = 0, silence = 0;
            for (int i = 0; i < DigitalCount; ++i) {
                if (!(mask & (1u << i))) continue;
                const InputConfig& c = state.input[i];
                if (c.action == InputAlarm) inputAlarm = true;
                if (c.action == InputForceOutput) force |= 1u << c.output;
                if (c.action == InputSilenceOutput) silence |= 1u << c.output;
            }
            const bool acknowledgeNow = ackRequested || acknowledgeInputNow;
            ackRequested = false;
            uint8_t outputs = 0, latched = 0, acknowledged = 0;
            for (int i = 0; i < OutputCount; ++i) {
                const bool active = (conditionInput >> i) & 1;
                bool on = active;
                if (state.output[i].latch) {
                    latches[i].update(active, acknowledgeNow);
                    on = latches[i].latched;
                    if (latches[i].latched) latched |= 1u << i;
                    if (latches[i].acknowledged) acknowledged |= 1u << i;
                }
                if (state.output[i].source == SourceOff) on = false;
                if (force & (1u << i)) on = true;
                if (silence & (1u << i)) on = false; // silence wins when two inputs target one DO
                if (on) outputs |= 1u << i;
                digitalWrite(outputPins[i], on ? HIGH : LOW);
            }
            const bool changed = !state.ready || state.inputs != mask || state.outputs != outputs ||
                                 state.conditions != conditionInput || state.latched != latched ||
                                 state.acknowledged != acknowledged || state.inputAlarm != inputAlarm;
            state.inputs = mask; state.outputs = outputs; state.conditions = conditionInput;
            state.latched = latched; state.acknowledged = acknowledged;
            state.inputAlarm = inputAlarm;
            if (sampleAnalog) {
                for (int i = 0; i < AnalogCount; ++i) {
                    state.adc[i] = adc[i]; state.measured[i] = measured[i];
                    state.value[i] = value[i]; state.analogFault[i] = analogFault[i];
                }
            }
            if (changed) state.sequence++;
            state.ready = true;
            xSemaphoreGive(mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
}

const char* validateOutputs(const OutputConfig (&output)[OutputCount]) {
    for (int i = 0; i < OutputCount; ++i) {
        const OutputConfig& o = output[i];
        if (o.source > SourceAn1) return "Unknown output source.";
        if (o.source == SourceOff) continue;
        if (o.source == SourceMetric) {
            if (o.metric >= MetricCount) return "Unknown 883M value.";
            if (o.axis > AxisAny) return "Unknown axis.";
        }
        if (o.direction > Below) return "Unknown direction.";
        if (!std::isfinite(o.threshold) || fabsf(o.threshold) > 1000000.0f) return "Threshold out of range.";
        if (!std::isfinite(o.hysteresis) || o.hysteresis < 0 || o.hysteresis > 1000000.0f) return "Hysteresis must be 0 or more.";
        if (o.delaySeconds > 3600) return "Delay must be 0-3600 s.";
    }
    return nullptr;
}

bool begin() {
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return false;
    loadAnalog(state.analog);
    loadInputs(state.input);
    loadOutputs(state.output);
    for (uint8_t pin : inputPins) pinMode(pin, INPUT);
    for (uint8_t pin : outputPins) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
    analogReadResolution(12);
    TaskHandle_t task = xTaskCreateStatic(sampler, "IO sampler", sizeof(samplerStack) / sizeof(StackType_t),
                                          nullptr, 2, samplerStack, &samplerTask);
    if (!task) return false;
    vTaskCoreAffinitySet(task, 1 << 0);
    return true;
}

Requests service(uint8_t conditions, bool alarmActive, bool sensorFault) {
    Requests requests;
    if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return requests;
    conditionInput = conditions;
    if (state.alarm != alarmActive || state.sensorFault != sensorFault) state.sequence++;
    state.alarm = alarmActive;
    state.sensorFault = sensorFault;
    requests.capture = captureEdge;
    requests.acknowledge = ackEdge;
    captureEdge = ackEdge = false;
    xSemaphoreGive(mutex);
    return requests;
}

void acknowledge() {
    if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    ackRequested = true;
    xSemaphoreGive(mutex);
}

void recordCapture(bool accepted) {
    if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    if (accepted) state.captureAccepted++; else state.captureRejected++;
    state.sequence++;
    xSemaphoreGive(mutex);
}

bool snapshot(Snapshot& out) {
    if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    out = state;
    xSemaphoreGive(mutex);
    return true;
}

bool setAnalogConfig(const AnalogConfig (&analog)[AnalogCount]) {
    Preferences store;
    if (!store.begin("io-cfg", false)) return false;
    bool ok = true;
    for (int i = 0; i < AnalogCount; ++i) {
        const String key = "an" + String(i) + "_";
        ok &= store.putString((key + "name").c_str(), analog[i].name) == strlen(analog[i].name);
        ok &= store.putString((key + "unit").c_str(), analog[i].unit) == strlen(analog[i].unit);
        ok &= store.putUChar((key + "mode").c_str(), analog[i].mode) == 1;
        ok &= store.putUChar((key + "model").c_str(), analog[i].model) == 1;
        ok &= store.putFloat((key + "min").c_str(), analog[i].minimum) == 4;
        ok &= store.putFloat((key + "max").c_str(), analog[i].maximum) == 4;
    }
    store.end();
    if (!ok) return false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    for (int i = 0; i < AnalogCount; ++i) {
        state.analog[i] = analog[i];
        state.adc[i] = 0; state.measured[i] = state.value[i] = 0; state.analogFault[i] = false;
    }
    state.sequence++; state.revision++;
    xSemaphoreGive(mutex);
    return true;
}

bool applyPcc420Preset(AnalogConfig& a) {
    static const float scales[] = {12.7f, 25.4f, 50.8f, 76.2f, 127.0f, 10.0f, 20.0f, 25.0f, 50.0f};
    if (a.model == 0 || a.model > 18) return false;
    a.mode = AnalogCurrent;
    a.minimum = 0;
    a.maximum = scales[(a.model - 1) % 9];
    snprintf(a.unit, sizeof(a.unit), "mm/s");
    return true;
}

bool setInputConfig(const InputConfig (&input)[DigitalCount]) {
    for (const InputConfig& c : input) if (c.action > InputAlarm || c.output >= OutputCount) return false;
    uint32_t first = 0, second = 0;
    for (int i = 0; i < DigitalCount; ++i) {
        const uint32_t packed = uint32_t(input[i].action) | (uint32_t(input[i].output) << 3);
        if (i < 6) first |= packed << (i * 5);
        else second |= packed << ((i - 6) * 5);
    }
    Preferences store;
    if (!store.begin("io-di", false)) return false;
    const bool ok = store.putUInt("di0_5", first) == 4 && store.putUInt("di6_10", second) == 4;
    store.end();
    if (!ok || xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    for (int i = 0; i < DigitalCount; ++i) state.input[i] = input[i];
    state.sequence++; state.revision++;
    xSemaphoreGive(mutex);
    return true;
}

bool setOutputConfig(const OutputConfig (&output)[OutputCount]) {
    if (validateOutputs(output)) return false;
    Preferences store;
    if (!store.begin("io-out", false)) return false;
    bool ok = true;
    for (int i = 0; i < OutputCount; ++i) {
        const String key = "do" + String(i) + "_";
        ok &= store.putUChar((key + "src").c_str(), output[i].source) == 1;
        ok &= store.putUChar((key + "met").c_str(), output[i].metric) == 1;
        ok &= store.putUChar((key + "axis").c_str(), output[i].axis) == 1;
        ok &= store.putUChar((key + "dir").c_str(), output[i].direction) == 1;
        ok &= store.putFloat((key + "thr").c_str(), output[i].threshold) == 4;
        ok &= store.putFloat((key + "hys").c_str(), output[i].hysteresis) == 4;
        ok &= store.putUShort((key + "dly").c_str(), output[i].delaySeconds) == 2;
        ok &= store.putBool((key + "lat").c_str(), output[i].latch) == 1;
    }
    store.end();
    if (!ok) return false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    for (int i = 0; i < OutputCount; ++i) state.output[i] = output[i];
    state.sequence++; state.revision++;
    xSemaphoreGive(mutex);
    return true;
}

void writeJson(JsonObject out, const Snapshot& s, bool formFields) {
    static const char* const sources[] = {"off", "alarm", "sensor_fault", "883m", "an0", "an1"};
    static const char* const axes[] = {"x", "y", "z", "any"};
    out["ready"] = s.ready;
    JsonArray inputs = out.createNestedArray("inputs");
    for (int i = 0; i < DigitalCount; ++i) inputs.add(bool((s.inputs >> i) & 1));
    if (formFields) {
        JsonArray inputRules = out.createNestedArray("input_rules");
        for (const InputConfig& c : s.input) {
            JsonObject row = inputRules.createNestedObject();
            row["action"] = c.action; row["output"] = c.output;
        }
    }
    out["input_alarm"] = s.inputAlarm;
    JsonArray outputs = out.createNestedArray("outputs");
    for (int i = 0; i < OutputCount; ++i) outputs.add(bool((s.outputs >> i) & 1));
    JsonArray rules = out.createNestedArray("output_rules");
    for (int i = 0; i < OutputCount; ++i) {
        const OutputConfig& o = s.output[i];
        JsonObject row = rules.createNestedObject();
        row["source"] = sources[o.source];
        if (formFields) {
            row["source_id"] = o.source; row["metric_index"] = o.metric; row["axis_id"] = o.axis; row["direction_id"] = o.direction;
            row["threshold_value"] = o.threshold; row["hysteresis_value"] = o.hysteresis; row["delay_value"] = o.delaySeconds;
        }
        if (o.source == SourceMetric) {
            row["metric"] = Metrics[o.metric].id;
            if (Metrics[o.metric].perAxis) row["axis"] = axes[o.axis];
        }
        if (o.source >= SourceMetric) {
            row["direction"] = o.direction == Below ? "below" : "above";
            row["threshold"] = o.threshold;
            row["hysteresis"] = o.hysteresis;
            row["delay_s"] = o.delaySeconds;
        }
        row["latch"] = o.latch;
        row["condition"] = bool((s.conditions >> i) & 1);
        row["latched"] = bool((s.latched >> i) & 1);
        row["acknowledged"] = bool((s.acknowledged >> i) & 1);
    }
    out["alarm"] = s.alarm;
    out["sensor_fault"] = s.sensorFault;
    out["capture_accepted"] = s.captureAccepted;
    out["capture_rejected"] = s.captureRejected;
    JsonArray analog = out.createNestedArray("analog");
    for (int i = 0; i < AnalogCount; ++i) {
        JsonObject row = analog.createNestedObject();
        row["name"] = s.analog[i].name;
        row["unit"] = s.analog[i].unit;
        row["mode"] = s.analog[i].mode == AnalogCurrent ? "4-20mA" : s.analog[i].mode == AnalogVoltage ? "0-10V" : "off";
        row["model"] = s.analog[i].model;
        row["minimum"] = s.analog[i].minimum;
        row["maximum"] = s.analog[i].maximum;
        if (s.analog[i].mode != AnalogOff) {
            row["input"] = s.measured[i];
            row["value"] = s.value[i];
            row["fault"] = s.analogFault[i];
            row["adc"] = s.adc[i];
        }
    }
}

bool fillModbusRegisters(uint16_t (&r)[10]) {
    Snapshot s;
    if (!snapshot(s) || !s.ready) return false;
    auto clamp16 = [](float v) -> uint16_t {
        if (!std::isfinite(v)) return 0;
        const long scaled = lroundf(v);
        return uint16_t(int16_t(scaled > 32767 ? 32767 : scaled < -32768 ? -32768 : scaled));
    };
    r[0] = s.inputs;
    r[1] = s.outputs;
    r[2] = (s.alarm ? 1 : 0) | (s.latched ? 2 : 0) | (s.acknowledged ? 4 : 0) | (s.sensorFault ? 8 : 0);
    r[3] = s.conditions;
    for (int i = 0; i < AnalogCount; ++i) {
        const bool on = s.analog[i].mode != AnalogOff;
        r[4 + i * 3] = on ? clamp16(s.value[i] * 10) : 0;
        r[5 + i * 3] = on ? clamp16(s.measured[i] * 100) : 0;
        r[6 + i * 3] = !on ? 0 : s.analogFault[i] ? 2 : 1;
    }
    return true;
}
}
#endif
