#include <Wilcoxon883MRTU.h>

Wilcoxon883MRTU sensor(Serial1);
bool connected=false,finished=false;
uint32_t lastRead=0,lastReported=0;

void setup() {
    Serial.begin(115200);
    Wilcoxon883MRTU::DiscoveryOptions options;
    // Defaults: ID 1..247, all manual baud rates and six UART formats.
    // Search is read-only. Connect one isolated sensor and no other RTU master.
    options.maxDurationMs=120000; // Two-minute initial search; may be incomplete.
    // If known, narrow the search for much faster results:
    // options.baud=115200;
    // options.firstUnit=1; options.lastUnit=16;
    // options.maxDurationMs=0; // Exhaust all candidates, potentially over an hour.
    if(!sensor.startDiscovery(18,17,14,options)) {
        Serial.println("Invalid pins or scan options");finished=true;
    } else Serial.println("Searching; send x to cancel. Sensor settings will not be written.");
}
void loop() {
    if(!finished) {
        if(Serial.available() && Serial.read()=='x')sensor.cancelDiscovery();
        auto state=sensor.pollDiscovery(); // At most one bounded candidate per call.
        const auto& progress=sensor.discoveryProgress();
        if(progress.attempts!=lastReported && progress.attempts%16==0){
            lastReported=progress.attempts;
            Serial.printf("%lu/%lu probes; baud=%lu ID=%u format=%s\n",
                (unsigned long)progress.attempts,(unsigned long)progress.totalCandidates,
                (unsigned long)progress.candidate.baud,unsigned(progress.candidate.unit),
                sensor.serialModeText(progress.candidate.mode));
        }
        if(state==Wilcoxon883MRTU::DiscoveryState::Found){
            const auto& found=sensor.discoveryResult();
            Serial.printf("Found %s %s: baud=%lu ID=%u format=%s\n",found.identity.vendor,
                found.identity.model,(unsigned long)found.settings.baud,unsigned(found.settings.unit),
                sensor.serialModeText(found.settings.mode));
            Serial.println("UART now uses the confirmed settings. No sensor setting was changed.");
            connected=true;finished=true;
        } else if(state!=Wilcoxon883MRTU::DiscoveryState::Searching){
            Serial.printf("Scan stopped: %s, probes=%lu. Check wiring or narrow/extend the search.\n",
                sensor.discoveryStateText(state),(unsigned long)progress.attempts);
            finished=true;
        }
    }
    if(connected && uint32_t(millis()-lastRead)>=2000){
        lastRead=millis();Wilcoxon883MRTU::Metrics metrics;
        auto result=sensor.readMetrics(metrics);
        if(result==Wilcoxon883MRTU::Result::Ok)
            Serial.printf("Velocity X=%.2f mm/s; temperature=%.1f C\n",metrics.velocityRmsMmS[0],metrics.temperatureC);
        else Serial.println(sensor.resultText(result));
    }
    delay(1);
}
