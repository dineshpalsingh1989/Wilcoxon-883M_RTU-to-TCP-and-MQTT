#include <Wilcoxon883MRTU.h>

Wilcoxon883MRTU sensor(Serial1);
bool initialized = false;

void setup() {
    Serial.begin(115200);
    // GPIO numbers, not physical header positions. DE and /RE are tied together.
    // Manual defaults: 9600, 8N1, slave 1. Match your actual sensor settings.
    initialized = sensor.begin(18, 17, 14, 9600, 1, SERIAL_8N1);
    if (!initialized) Serial.println("Check GPIO, baud rate and UART configuration.");
}
void loop() {
    if (initialized) {
        Wilcoxon883MRTU::Metrics metrics;
        auto result = sensor.readMetrics(metrics);
        if (result == Wilcoxon883MRTU::Result::Ok) {
            Serial.printf("Velocity RMS mm/s X=%.2f Y=%.2f Z=%.2f; temperature=%.1f C\n",
                metrics.velocityRmsMmS[0], metrics.velocityRmsMmS[1],
                metrics.velocityRmsMmS[2], metrics.temperatureC);
        } else {
            Serial.printf("Read failed: %s; exception=%u\n",
                Wilcoxon883MRTU::resultText(result), unsigned(sensor.exceptionCode()));
        }
    }
    delay(2000);
}
