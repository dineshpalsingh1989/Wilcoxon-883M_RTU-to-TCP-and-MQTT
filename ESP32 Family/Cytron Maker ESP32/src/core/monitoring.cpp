#include "core/modules.h"

bool firmwareUpdateActive = false;
bool backgroundPausedForUpdate = false;
namespace {
struct Trend { uint32_t ms; float x, y, z, temp; };
struct Capture { char timestamp[21]; char axis; char result[32]; char transmission[40]; };
Trend trends[120];
Capture captures[20];
unsigned trendIndex = 0, trendCount = 0, captureIndex = 0, captureCount = 0;
uint32_t lastSuccess = 0, readCount = 0, failures = 0, timeouts = 0, consecutive = 0;
uint8_t lastError = 0;
bool haveReading = false, alarmEnabled = false, alarmActive = false, alarmPending = false;
float velocityLimit = 4.5f, temperatureLimit = 80.0f;
uint32_t delayMs = 10000, alarmSince = 0;
void sendJson(JsonDocument& doc) { String payload; serializeJson(doc, payload); server.send(200, "application/json", payload); }
}

void setupMonitoring() {
    Preferences prefs;
    prefs.begin("alarms", true);
    alarmEnabled = prefs.getBool("enabled", false);
    velocityLimit = prefs.getFloat("velocity", 4.5f);
    temperatureLimit = prefs.getFloat("temperature", 80.0f);
    delayMs = prefs.getUInt("delay", 10000);
    prefs.end();
}

void recordMetricsHealth(bool success, uint8_t error) {
    readCount++;
    if (!success) {
        failures++; consecutive++; lastError = error;
        if (error == RTU_ERROR_TIMEOUT) timeouts++;
        // A communication gap must not count toward a sustained alarm.
        alarmPending = false; alarmActive = false;
        return;
    }
    if (haveReading && millis() - lastSuccess >= std::max<uint32_t>(15000, metricsReadInterval_ms * 3)) { alarmPending = false; alarmActive = false; }
    haveReading = true; lastSuccess = millis(); consecutive = 0; lastError = 0;
    trends[trendIndex] = {lastSuccess, veloRmsX, veloRmsY, veloRmsZ, temperature};
    trendIndex = (trendIndex + 1) % 120;
    if (trendCount < 120) trendCount++;
    bool exceeded = veloRmsX >= velocityLimit || veloRmsY >= velocityLimit || veloRmsZ >= velocityLimit || temperature >= temperatureLimit;
    if (alarmEnabled && exceeded) {
        if (!alarmPending) { alarmSince = millis(); alarmPending = true; }
        alarmActive = millis() - alarmSince >= delayMs;
    } else { alarmPending = false; alarmActive = false; }
}

void recordCaptureResult(Axis axis, const char* result, const char* transmission) {
    Capture& entry = captures[captureIndex];
    snprintf(entry.timestamp, sizeof(entry.timestamp), "%s", getTimestamp().c_str());
    entry.axis = axis == AXIS_X ? 'X' : axis == AXIS_Y ? 'Y' : axis == AXIS_Z ? 'Z' : '?';
    snprintf(entry.result, sizeof(entry.result), "%s", result);
    snprintf(entry.transmission, sizeof(entry.transmission), "%s", transmission);
    captureIndex = (captureIndex + 1) % 20;
    if (captureCount < 20) captureCount++;
}

void handleMonitoring() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { server.send(503, "text/plain", "Device busy"); return; }
    bool updating = firmwareUpdateActive;
    xSemaphoreGive(dataMutex);
    if (updating) { server.send(503, "text/plain", "Monitoring paused for firmware update"); return; }
    DynamicJsonDocument doc(24576);
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { server.send(503, "text/plain", "Device busy"); return; }
    uint32_t age = millis() - lastSuccess;
    const uint32_t staleAfter = std::max<uint32_t>(15000, metricsReadInterval_ms * 3);
    const bool fresh = haveReading && consecutive == 0 && age < staleAfter;
    JsonObject health = doc.createNestedObject("health");
    health["state"] = !haveReading ? "Waiting for sensor" : consecutive ? "Communication error" : !fresh ? "Readings stale" : "Connected";
    health["has_reading"] = haveReading; health["age_ms"] = age;
    health["reads"] = readCount; health["failures"] = failures; health["timeouts"] = timeouts;
    health["consecutive_errors"] = consecutive; health["last_error"] = lastError;
    health["capturing"] = currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum;
    health["updating"] = firmwareUpdateActive;
    JsonObject alarm = doc.createNestedObject("alarm");
    alarm["enabled"] = alarmEnabled; alarm["velocity"] = velocityLimit; alarm["temperature"] = temperatureLimit; alarm["delay_seconds"] = delayMs / 1000;
    alarm["state"] = !alarmEnabled ? "Disabled" : !fresh ? "Unknown - sensor data unavailable" : alarmActive ? "Alarm active" : alarmPending ? "Threshold exceeded - pending" : "Within limits";
    JsonArray history = doc.createNestedArray("trends");
    for (unsigned i = 0; i < trendCount; i++) {
        const Trend& point = trends[(trendIndex + 120 - trendCount + i) % 120];
        JsonObject row = history.createNestedObject();
        row["age_ms"] = uint32_t(millis() - point.ms); row["x"] = point.x; row["y"] = point.y; row["z"] = point.z; row["temperature"] = point.temp;
    }
    JsonArray events = doc.createNestedArray("captures");
    for (unsigned i = 0; i < captureCount; i++) {
        const Capture& entry = captures[(captureIndex + 19 - i) % 20];
        JsonObject row = events.createNestedObject();
        row["time"] = entry.timestamp; row["axis"] = String(entry.axis); row["result"] = entry.result; row["transmission"] = entry.transmission;
    }
    xSemaphoreGive(dataMutex);
    if (doc.overflowed()) { server.send(500, "text/plain", "Monitoring snapshot too large"); return; }
    sendJson(doc);
}

void handleAlarmSettings() {
    DynamicJsonDocument body(512);
    if (deserializeJson(body, server.arg("plain")) || !body["enabled"].is<bool>() || !body["velocity"].is<float>() || !body["temperature"].is<float>() || !body["delay_seconds"].is<unsigned>()) {
        server.send(400, "text/plain", "Provide enabled, velocity, temperature and delay_seconds"); return;
    }
    float velocity = body["velocity"], temp = body["temperature"];
    unsigned delay = body["delay_seconds"];
    if (!std::isfinite(velocity) || velocity <= 0 || velocity > 1000 || !std::isfinite(temp) || temp < -40 || temp > 200 || delay > 3600) {
        server.send(400, "text/plain", "Invalid alarm limits"); return;
    }
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { server.send(503, "text/plain", "Device busy"); return; }
    alarmEnabled = body["enabled"]; velocityLimit = velocity; temperatureLimit = temp; delayMs = delay * 1000;
    alarmPending = false; alarmActive = false;
    xSemaphoreGive(dataMutex);
    Preferences prefs;
    if (!prefs.begin("alarms", false)) { server.send(500, "text/plain", "Unable to save alarms"); return; }
    bool saved = prefs.putBool("enabled", alarmEnabled) && prefs.putFloat("velocity", velocity) && prefs.putFloat("temperature", temp) && prefs.putUInt("delay", delay * 1000);
    prefs.end();
    server.send(saved ? 200 : 500, "text/plain", saved ? "Alarm settings saved" : "Unable to persist all alarm settings");
}
