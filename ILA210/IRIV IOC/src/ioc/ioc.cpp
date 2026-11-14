#include "ioc/ioc.h"
#include "ioc/logic.h"
#include "ioc/pages.h"
#include "platform/platform.h"
#include <ArduinoJson.h>
#include <cmath>
#if GATEWAY_ETHERNET
#include <LittleFS.h>
#endif

namespace ioc {
#if GATEWAY_ETHERNET
namespace {
constexpr int DigitalCount = 11, AnalogCount = 2, OutputCount = 2;
const uint8_t inputPins[] = {PIN_DI0, PIN_DI1, PIN_DI2, PIN_DI3, PIN_DI4, PIN_DI5,
                            PIN_DI6, PIN_DI7, PIN_DI8, PIN_DI9, PIN_DI10};
const uint8_t analogPins[] = {PIN_AN0, PIN_AN1};
const uint8_t outputPins[] = {PIN_DO2, PIN_DO3};
struct DigitalConfig { char name[25] = ""; uint8_t action = 0; bool activeLow = false; uint32_t delayMs = 500; };
struct AnalogConfig {
    char name[25] = "", unit[13] = "";
    uint8_t mode = 0; // Disabled, 4-20 mA, 0-10 V. Physical switch must match.
    float minimum = 0, maximum = 100, low = 10, high = 90, hysteresis = 1;
    bool lowEnabled = false, highEnabled = false, faultAlarm = false;
    uint32_t delayMs = 1000;
};
struct OutputConfig { uint8_t source = 0, input = 0; }; // Off, active alarm, latched alarm, sounder, follow DI.
struct Config {
    uint32_t revision = 0;
    float referenceMv = 3320;
    DigitalConfig digital[DigitalCount];
    AnalogConfig analog[AnalogCount];
    OutputConfig outputs[OutputCount];
};
struct State {
    uint32_t sampledAt = 0, sequence = 0, captureAccepted = 0, captureRejected = 0, sources = 0;
    bool active[DigitalCount] = {}, digitalAlarm[DigitalCount] = {};
    float measured[AnalogCount] = {}, value[AnalogCount] = {};
    uint16_t adc[AnalogCount] = {};
    bool fault[AnalogCount] = {}, analogAlarm[AnalogCount] = {}, outputs[OutputCount] = {};
    bool sensorAlert = false, sensorAlarm = false, sensorFault = false, ready = false;
    gateway::SensorHealth sensorHealth;
    AlarmLatch latch;
};
Config config;
State state;
SemaphoreHandle_t mutex = nullptr;
bool acknowledgeRequested = false;
bool (*captureCallback)() = nullptr;
StackType_t samplerStack[8192 / sizeof(StackType_t)];
StaticTask_t samplerTask;

void writeConfig(JsonDocument& doc, const Config& cfg) {
    doc["revision"] = cfg.revision; doc["reference_mv"] = cfg.referenceMv;
    JsonArray digital = doc.createNestedArray("digital");
    for (const auto& d : cfg.digital) {
        JsonObject row = digital.createNestedObject();
        row["name"] = String(d.name); row["action"] = d.action;
        row["active_low"] = d.activeLow; row["delay_ms"] = d.delayMs;
    }
    JsonArray analog = doc.createNestedArray("analog");
    for (const auto& a : cfg.analog) {
        JsonObject row = analog.createNestedObject();
        row["name"] = String(a.name); row["unit"] = String(a.unit); row["mode"] = a.mode;
        row["minimum"] = a.minimum; row["maximum"] = a.maximum;
        row["low"] = a.low; row["high"] = a.high; row["hysteresis"] = a.hysteresis;
        row["low_enabled"] = a.lowEnabled; row["high_enabled"] = a.highEnabled;
        row["fault_alarm"] = a.faultAlarm; row["delay_ms"] = a.delayMs;
    }
    JsonArray outputs = doc.createNestedArray("outputs");
    for (const auto& o : cfg.outputs) {
        JsonObject row = outputs.createNestedObject(); row["source"] = o.source; row["input"] = o.input;
    }
}
bool number(JsonVariantConst v, float minimum, float maximum) {
    return v.is<float>() && std::isfinite(v.as<float>()) && v.as<float>() >= minimum && v.as<float>() <= maximum;
}
bool text(JsonVariantConst v, size_t maximum) {
    if (!v.is<const char*>() || strlen(v.as<const char*>()) > maximum) return false;
    for (const char* c = v.as<const char*>(); *c; ++c) if (uint8_t(*c) < 32) return false;
    return true;
}
bool integer(JsonVariantConst v, uint32_t maximum) { return v.is<uint32_t>() && v.as<uint32_t>() <= maximum; }
bool readConfig(JsonVariantConst doc, Config& cfg) {
    if (!integer(doc["revision"], UINT32_MAX) || !number(doc["reference_mv"], 3000, 3600) ||
        doc["digital"].size() != DigitalCount || doc["analog"].size() != AnalogCount || doc["outputs"].size() != OutputCount) return false;
    cfg.revision = doc["revision"]; cfg.referenceMv = doc["reference_mv"];
    for (int i = 0; i < DigitalCount; ++i) {
        JsonVariantConst row = doc["digital"][i];
        if (!text(row["name"], 24) || !integer(row["action"], 4) || !row["active_low"].is<bool>() || !integer(row["delay_ms"], 60000)) return false;
        auto& d = cfg.digital[i]; snprintf(d.name, sizeof(d.name), "%s", row["name"].as<const char*>());
        d.action = row["action"]; d.activeLow = row["active_low"]; d.delayMs = row["delay_ms"];
    }
    for (int i = 0; i < AnalogCount; ++i) {
        JsonVariantConst row = doc["analog"][i];
        if (!text(row["name"], 24) || !text(row["unit"], 12) || !integer(row["mode"], 2) ||
            !number(row["minimum"], -1000000, 1000000) || !number(row["maximum"], -1000000, 1000000) ||
            row["minimum"].as<float>() >= row["maximum"].as<float>() ||
            !number(row["low"], -1000000, 1000000) || !number(row["high"], -1000000, 1000000) ||
            !number(row["hysteresis"], 0, 1000000) || !integer(row["delay_ms"], 60000) ||
            !row["low_enabled"].is<bool>() || !row["high_enabled"].is<bool>() || !row["fault_alarm"].is<bool>()) return false;
        auto& a = cfg.analog[i];
        snprintf(a.name, sizeof(a.name), "%s", row["name"].as<const char*>());
        snprintf(a.unit, sizeof(a.unit), "%s", row["unit"].as<const char*>());
        a.mode = row["mode"]; a.minimum = row["minimum"]; a.maximum = row["maximum"];
        a.low = row["low"]; a.high = row["high"]; a.hysteresis = row["hysteresis"];
        a.lowEnabled = row["low_enabled"]; a.highEnabled = row["high_enabled"];
        a.faultAlarm = row["fault_alarm"]; a.delayMs = row["delay_ms"];
        if (a.lowEnabled && (a.low < a.minimum || a.low > a.maximum)) return false;
        if (a.highEnabled && (a.high < a.minimum || a.high > a.maximum)) return false;
        if (a.hysteresis > a.maximum - a.minimum ||
            (a.lowEnabled && a.highEnabled && (a.low >= a.high || a.hysteresis * 2 >= a.high - a.low))) return false;
    }
    for (int i = 0; i < OutputCount; ++i) {
        JsonVariantConst row = doc["outputs"][i];
        if (!integer(row["source"], 4) || !integer(row["input"], 10)) return false;
        cfg.outputs[i].source = row["source"]; cfg.outputs[i].input = row["input"];
    }
    return true;
}
void defaults() {
    for (int i = 0; i < DigitalCount; ++i) snprintf(config.digital[i].name, 25, "DI%d", i);
    for (int i = 0; i < AnalogCount; ++i) snprintf(config.analog[i].name, 25, "AN%d", i);
}
bool snapshot(Config& cfg, State& current) {
    if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    cfg = config; current = state; xSemaphoreGive(mutex); return true;
}
void writeState(JsonDocument& doc, const State& current) {
    doc["ready"] = current.ready; doc["fresh"] = current.ready && uint32_t(millis() - current.sampledAt) < 1000; doc["sequence"] = current.sequence;
    doc["age_ms"] = uint32_t(millis() - current.sampledAt);
    doc["alarm"] = current.sources != 0; doc["latched"] = current.latch.latched;
    doc["acknowledged"] = current.latch.acknowledged;
    doc["sensor_fault"] = current.sensorFault; doc["fault_alarm_enabled"] = current.sensorHealth.faultAlarmEnabled;
    doc["sensor_alert"] = current.sensorAlert; doc["sensor_alarm"] = current.sensorAlarm;
    doc["capture_accepted"] = current.captureAccepted; doc["capture_rejected"] = current.captureRejected;
    JsonArray digital = doc.createNestedArray("digital");
    for (int i = 0; i < DigitalCount; ++i) {
        JsonObject row = digital.createNestedObject(); row["active"] = current.active[i]; row["alarm"] = current.digitalAlarm[i];
    }
    JsonArray analog = doc.createNestedArray("analog");
    for (int i = 0; i < AnalogCount; ++i) {
        JsonObject row = analog.createNestedObject(); row["adc"] = current.adc[i];
        row["measured"] = current.measured[i]; row["value"] = current.value[i];
        row["fault"] = current.fault[i]; row["alarm"] = current.analogAlarm[i];
    }
    JsonArray outputs = doc.createNestedArray("outputs"); for (bool output : current.outputs) outputs.add(output);
}
void sampler(void*) {
    Debounce digital[DigitalCount]; Delay digitalDelay[DigitalCount], analogDelay[AnalogCount];
    Thresholds thresholds[AnalogCount];
    uint32_t revision = UINT32_MAX, analogAt = 0;
    for (;;) {
        Config cfg; State next;
        if (!snapshot(cfg, next)) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        const uint32_t now = millis();
        if (revision != cfg.revision) {
            for (auto& d : digital) d = Debounce();
            for (auto& d : digitalDelay) d = Delay();
            for (auto& d : analogDelay) d = Delay();
            for (auto& t : thresholds) t = Thresholds();
            for (int i = 0; i < AnalogCount; ++i) { next.fault[i] = false; next.analogAlarm[i] = false; next.measured[i] = next.value[i] = 0; next.adc[i] = 0; }
            revision = cfg.revision; analogAt = now - 100;
        }
        bool capture = false, acknowledge = false;
        next.sensorFault = next.sensorHealth.lost(now);
        uint32_t sources = (next.sensorFault && next.sensorHealth.faultAlarmEnabled ? 1u << 18 : 0) | (next.sensorAlert ? 1u << 16 : 0) | (next.sensorAlarm ? 1u << 17 : 0);
        for (int i = 0; i < DigitalCount; ++i) {
            const auto& d = cfg.digital[i];
            const bool raw = (digitalRead(inputPins[i]) != LOW) != d.activeLow;
            const bool pressed = digital[i].update(raw, now);
            next.active[i] = digital[i].stable;
            next.digitalAlarm[i] = digitalDelay[i].update(d.action == 2 && next.active[i], now, d.delayMs);
            if (next.digitalAlarm[i]) sources |= 1u << i;
            if (pressed && d.action == 3) capture = true;
            if (pressed && d.action == 4) acknowledge = true;
        }
        if (uint32_t(now - analogAt) >= 100) {
            analogAt = now;
            for (int i = 0; i < AnalogCount; ++i) {
                const auto& a = cfg.analog[i];
                if (!a.mode) { next.analogAlarm[i] = false; continue; }
                uint32_t total = 0; for (int n = 0; n < 8; ++n) total += analogRead(analogPins[i]);
                next.adc[i] = total / 8;
                next.measured[i] = a.mode == 1 ? terminalMilliamps(next.adc[i], cfg.referenceMv) : terminalVolts(next.adc[i], cfg.referenceMv);
                next.value[i] = engineeringValue(next.measured[i], a.mode == 1, a.minimum, a.maximum);
                next.fault[i] = next.adc[i] >= 4090 || (a.mode == 1 && (next.measured[i] < 3.6f || next.measured[i] > 21.0f));
                const bool limit = thresholds[i].update(next.value[i], a.lowEnabled, a.low, a.highEnabled, a.high, a.hysteresis);
                next.analogAlarm[i] = analogDelay[i].update(limit || (a.faultAlarm && next.fault[i]), now, a.delayMs);
            }
        }
        for (int i = 0; i < AnalogCount; ++i) if (next.analogAlarm[i]) sources |= 1u << (11 + i);
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) continue;
        if (config.revision != cfg.revision) { xSemaphoreGive(mutex); continue; }
        acknowledge |= acknowledgeRequested; acknowledgeRequested = false;
        next.sources = sources; next.latch.update(sources, acknowledge);
        for (int i = 0; i < OutputCount; ++i) {
            const auto& o = cfg.outputs[i];
            next.outputs[i] = outputActive(o.source, sources, next.latch,
                                           cfg.digital[o.input].action != 0, next.active[o.input]);
            digitalWrite(outputPins[i], next.outputs[i] ? HIGH : LOW);
        }
        // MQTT notification sequence tracks meaningful transitions; analog values have a heartbeat.
        bool changed = state.sensorFault != next.sensorFault || state.sources != next.sources || state.latch.latched != next.latch.latched || state.latch.acknowledged != next.latch.acknowledged;
        for (int i = 0; i < DigitalCount; ++i) changed |= state.active[i] != next.active[i];
        for (int i = 0; i < OutputCount; ++i) changed |= state.outputs[i] != next.outputs[i];
        next.sequence += changed; next.sampledAt = now; next.ready = true;
        // Preserve sensor updates arriving since the snapshot; apply them on the next sample.
        next.sensorHealth = state.sensorHealth;
        next.sensorAlert = state.sensorAlert; next.sensorAlarm = state.sensorAlarm;
        state = next; xSemaphoreGive(mutex);
        if (capture && captureCallback) {
            const bool accepted = captureCallback();
            xSemaphoreTake(mutex, portMAX_DELAY);
            if (accepted) ++state.captureAccepted; else ++state.captureRejected;
            ++state.sequence; xSemaphoreGive(mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
bool persist(const Config& cfg) {
    DynamicJsonDocument doc(8192); writeConfig(doc, cfg);
    if (doc.overflowed() || !gw::fsLock()) return false;
    File file = LittleFS.open("/ioc.tmp", "w");
    const size_t required = measureJson(doc);
    bool ok = file && serializeJson(doc, file) == required;
    if (file) file.close();
    if (ok) ok = LittleFS.rename("/ioc.tmp", "/ioc.json");
    if (!ok) LittleFS.remove("/ioc.tmp");
    gw::fsUnlock(); return ok;
}
void jsonResponse(WebServer& server, JsonDocument& doc) {
    if (doc.overflowed()) { server.send(503, "application/json", "{\"error\":\"Response capacity exceeded\"}"); return; }
    String body; serializeJson(doc, body); server.sendHeader("Cache-Control", "no-store"); server.send(200, "application/json", body);
}
}
#endif

bool begin(bool (*requestCapture)()) {
#if GATEWAY_ETHERNET
    captureCallback = requestCapture;
    mutex = xSemaphoreCreateMutex(); if (!mutex) return false;
    defaults();
    if (gw::fsLock()) {
        File file = LittleFS.open("/ioc.json", "r"); DynamicJsonDocument doc(8192);
        Config loaded;
        if (file && !deserializeJson(doc, file) && readConfig(doc.as<JsonVariantConst>(), loaded)) config = loaded;
        if (file) file.close(); gw::fsUnlock();
    }
    for (uint8_t pin : inputPins) pinMode(pin, INPUT);
    for (uint8_t pin : outputPins) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
    analogReadResolution(12);
    TaskHandle_t task = xTaskCreateStatic(sampler, "IOC sampler", sizeof(samplerStack) / sizeof(StackType_t),
                                         nullptr, 2, samplerStack, &samplerTask);
    if (!task) return false;
    vTaskCoreAffinitySet(task, 1 << 0);
    return true;
#else
    (void)requestCapture; return true;
#endif
}
void setSensorAlarms(bool alert, bool alarm, const gateway::SensorHealth& health) {
#if GATEWAY_ETHERNET
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        state.sensorHealth = health; state.sensorAlert = alert; state.sensorAlarm = alarm; xSemaphoreGive(mutex);
    }
#else
    (void)alert; (void)alarm; (void)health;
#endif
}
void registerRoutes(WebServer& server) {
    for (const char* route : {"/ioc", "/ioc/inputs", "/ioc/analog", "/ioc/outputs"})
        server.on(route, HTTP_GET, [&server]() { server.send_P(200, "text/html; charset=utf-8", IOC_PAGE); });
    server.on("/api/ioc", HTTP_GET, [&server]() {
#if GATEWAY_ETHERNET
        Config cfg; State current;
        if (!snapshot(cfg, current)) { server.send(503, "application/json", "{\"error\":\"I/O busy\"}"); return; }
        DynamicJsonDocument doc(12288); doc["supported"] = true;
        JsonObject conf = doc.createNestedObject("config");
        DynamicJsonDocument configDoc(8192); writeConfig(configDoc, cfg); conf.set(configDoc.as<JsonObject>());
        JsonObject status = doc.createNestedObject("state");
        DynamicJsonDocument statusDoc(4096); writeState(statusDoc, current); status.set(statusDoc.as<JsonObject>());
        jsonResponse(server, doc);
#else
        server.send(200, "application/json", "{\"supported\":false}");
#endif
    });
    server.on("/api/ioc/config", HTTP_POST, [&server]() {
#if GATEWAY_ETHERNET
        if (server.arg("plain").length() > 7000) { server.send(413, "text/plain", "Configuration too large."); return; }
        DynamicJsonDocument doc(8192); Config candidate;
        if (deserializeJson(doc, server.arg("plain")) || !readConfig(doc.as<JsonVariantConst>(), candidate)) {
            server.send(400, "text/plain", "Invalid I/O settings. Check ranges, thresholds and hysteresis."); return;
        }
        Config previous; State current;
        if (!snapshot(previous, current)) { server.send(503, "text/plain", "I/O busy."); return; }
        if (candidate.revision != previous.revision) { server.send(409, "text/plain", "Settings changed. Reload this page before saving."); return; }
        ++candidate.revision;
        if (!persist(candidate)) { server.send(503, "text/plain", "Settings could not be stored; no changes applied."); return; }
        xSemaphoreTake(mutex, portMAX_DELAY); config = candidate; xSemaphoreGive(mutex);
        server.send(200, "text/plain", "I/O settings saved and applied.");
#else
        server.send(400, "text/plain", "IOC hardware is unavailable on ESP32-S3.");
#endif
    });
    server.on("/api/ioc/acknowledge", HTTP_POST, [&server]() {
#if GATEWAY_ETHERNET
        if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) { server.send(503, "text/plain", "I/O busy."); return; }
        acknowledgeRequested = true; xSemaphoreGive(mutex);
        server.send(200, "text/plain", "Acknowledgement requested. Active conditions remain alarms.");
#else
        server.send(400, "text/plain", "IOC hardware is unavailable on ESP32-S3.");
#endif
    });
}
void serviceMqtt(WilcoxonMqttClient& client, const String& baseTopic) {
#if GATEWAY_ETHERNET
    static uint32_t sentSequence = UINT32_MAX, sentAt = 0;
    if (!client.connected()) return;
    Config cfg; State current; if (!snapshot(cfg, current) || !current.ready) return;
    if (current.sequence == sentSequence && uint32_t(millis() - sentAt) < 10000) return;
    DynamicJsonDocument doc(4096); writeState(doc, current);
    doc["type"] = "ioc_status"; doc["device_id"] = gw::deviceId();
    char payload[2048];
    if (doc.overflowed() || measureJson(doc) >= sizeof(payload)) return;
    const size_t size = serializeJson(doc, payload, sizeof(payload));
    if (client.publish((baseTopic + "/io").c_str(), reinterpret_cast<const uint8_t*>(payload), size, false)) {
        sentSequence = current.sequence; sentAt = millis();
    }
#else
    (void)client; (void)baseTopic;
#endif
}
}
