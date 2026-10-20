#include <WiFi.h>
#include <Wilcoxon883MRTU.h>
#include <Wilcoxon883MTCP.h>

// Replace with your network. Do not commit real credentials.
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Holding registers 0-99 use the same addresses and scaling as the gateway
// for the values readMetrics() provides. Register 99 holds the last RTU
// result code (0 = OK); values keep their last good reading after a failure.
const uint16_t ACCEL_RMS[3] = {0, 20, 40};   // g x100
const uint16_t VELOCITY_RMS[3] = {4, 24, 44}; // mm/s x10
const uint16_t TEMPERATURE = 60;             // degrees C x10, signed
const uint16_t READ_STATUS = 99;

Wilcoxon883MRTU sensor(Serial1);
uint16_t registers[100];
Wilcoxon883MTCP::RegisterBlocks<1> bank;
Wilcoxon883MTCP plcServer(bank);
bool sensorReady = false;
uint32_t lastRead = 0;

void setup() {
    Serial.begin(115200);
    // GPIO numbers, not physical header positions. DE and /RE are tied together.
    sensorReady = sensor.begin(18, 17, 14, 9600, 1, SERIAL_8N1);
    if (!sensorReady) Serial.println("Check GPIO, baud rate and UART configuration.");
    bank.addHolding(0, registers, 100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print('.'); }
    // Port 502, up to 4 PLC connections, any unit ID.
    if (plcServer.begin(502, 4)) Serial.printf("\nModbus TCP on %s:502\n", WiFi.localIP().toString().c_str());
}

void loop() {
    // Single task: registers are only changed here, so no lock is needed.
    plcServer.poll();
    if (!sensorReady || millis() - lastRead < 1000) return;
    lastRead = millis();
    Wilcoxon883MRTU::Metrics metrics;
    // Short timeouts keep PLC requests flowing if the sensor stops answering.
    auto result = sensor.readMetrics(metrics, 300);
    if (result == Wilcoxon883MRTU::Result::Ok) {
        for (int axis = 0; axis < 3; ++axis) {
            registers[ACCEL_RMS[axis]] = uint16_t(lroundf(metrics.accelerationRmsG[axis] * 100));
            registers[VELOCITY_RMS[axis]] = uint16_t(lroundf(metrics.velocityRmsMmS[axis] * 10));
        }
        registers[TEMPERATURE] = uint16_t(int16_t(lroundf(metrics.temperatureC * 10)));
    } else {
        Serial.printf("Read failed: %s\n", Wilcoxon883MRTU::resultText(result));
    }
    registers[READ_STATUS] = uint16_t(result);
}
