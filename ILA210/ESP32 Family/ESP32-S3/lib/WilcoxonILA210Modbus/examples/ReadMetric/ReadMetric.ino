#include <WilcoxonILA210RTU.h>

// Existing board wiring; no GPIO direction control on the IRIV transceiver.
#if defined(ARDUINO_ARCH_RP2040)
#define SENSOR_PORT Serial2
constexpr int RxPin = PIN_SERIAL2_RX, TxPin = PIN_SERIAL2_TX, DirectionPin = -1;
#else
#define SENSOR_PORT Serial1
constexpr int RxPin = 16, TxPin = 18, DirectionPin = 10;
#endif
constexpr uint32_t Baud = 9600; // Manual factory default; match the configured sensor.
WilcoxonILA210RTU sensor;
bool ready = false;

void transmit() { if (DirectionPin >= 0) digitalWrite(DirectionPin, HIGH); }
void receive() { if (DirectionPin >= 0) digitalWrite(DirectionPin, LOW); }

void setup() {
    Serial.begin(115200);
    if (DirectionPin >= 0) pinMode(DirectionPin, OUTPUT);
    receive();
    #if defined(ARDUINO_ARCH_RP2040)
    SENSOR_PORT.setRX(RxPin); SENSOR_PORT.setTX(TxPin);
    SENSOR_PORT.begin(Baud, SERIAL_8N1);
#else
    SENSOR_PORT.begin(Baud, SERIAL_8N1, RxPin, TxPin);
#endif
    sensor.preTransmission(transmit);
    sensor.postTransmission(receive);
    ready = sensor.begin(1, SENSOR_PORT, Baud); // Match the configured slave ID.
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
