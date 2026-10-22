#include <WilcoxonILA210RTU.h>

// Existing ILA ESP32-S3 gateway wiring. Confirm your RS485 transceiver first.
constexpr int RxPin = 16;
constexpr int TxPin = 18;
constexpr int DirectionPin = 10;
constexpr uint32_t Baud = 9600; // Manual factory default; match the configured sensor.
WilcoxonILA210RTU sensor;
bool ready = false;

void transmit() { digitalWrite(DirectionPin, HIGH); }
void receive() { digitalWrite(DirectionPin, LOW); }

void setup() {
    Serial.begin(115200);
    pinMode(DirectionPin, OUTPUT);
    receive();
    Serial1.begin(Baud, SERIAL_8N1, RxPin, TxPin);
    sensor.preTransmission(transmit);
    sensor.postTransmission(receive);
    ready = sensor.begin(1, Serial1, Baud); // Match the configured slave ID.
    if (!ready) Serial.println("Invalid RTU configuration");
}

void loop() {
    if (ready) {
        float value = 0;
        const uint8_t result = sensor.readMetric(0, value);
        if (result == 0) Serial.printf("AccelRMS1: %.2f g\n", value);
        else Serial.printf("RTU error: 0x%02X\n", result);
    }
    delay(500);
}
