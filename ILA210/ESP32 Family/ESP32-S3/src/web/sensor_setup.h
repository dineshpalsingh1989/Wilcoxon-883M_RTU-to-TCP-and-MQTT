#pragma once
#include <ILASetup.h>
#include "sensor_setup_page.h"

// Included after firmware globals/functions. Only backgroundLoop owns the UART.
struct SensorSetupLink {
    void connect(uint32_t baud, uint8_t id) {
        SENSOR_SERIAL.end();
#if GATEWAY_ETHERNET
        SENSOR_SERIAL.setRX(RXD_PIN); SENSOR_SERIAL.setTX(TXD_PIN);
        SENSOR_SERIAL.begin(baud, SERIAL_8N1);
#else
        SENSOR_SERIAL.begin(baud, SERIAL_8N1, RXD_PIN, TXD_PIN);
#endif
        node.begin(id, SENSOR_SERIAL, baud);
        node.setTimeout(250);
    }
    bool probe(uint32_t baud, uint8_t id) {
        if (node.readHoldingRegisters(0, 5)) return false;
        // Validate the whole settings signature, not any arbitrary RTU reply.
        return node.getResponseBuffer(0)==wilcoxonila210::baudIndex(baud)+9 &&
            node.getResponseBuffer(1)==8 && node.getResponseBuffer(2)==0 &&
            node.getResponseBuffer(3)==1 && node.getResponseBuffer(4)==id;
    }
    void write(uint16_t address, uint16_t value) { node.writeSingleRegister(address,value); }
    bool save(uint32_t baud,uint8_t id) {
        GatewayPreferences settings;
        bool ok=settings.begin("rs485-cfg",false);
        if(ok) ok=settings.putUChar("slave_id",id) && settings.putLong("baud_rate",baud) && settings.commit();
        settings.end();
        xSemaphoreTake(dataMutex,portMAX_DELAY);
        modbus_slave_id_val=id;modbus_baud_rate_val=baud;
        sensorHealth.seen=false;sensorHealth.startedAt=millis();metricsValid=false;alarmMetricValid=false;
        newWaveformAvailable=false;newSpectrumAvailable=false;
        ++sensorConnectionGeneration;
        xSemaphoreGive(dataMutex);
        return ok;
    }
};
static SensorSetupLink sensorSetupLink;
static wilcoxonila210::SetupController<SensorSetupLink> sensorSetupController(sensorSetupLink);
struct SensorSetupRequest { uint8_t mode=0,id=1,first=1,last=16; uint32_t baud=9600; };
static SensorSetupRequest sensorSetupRequest;
static bool sensorSetupQueued=false,sensorSetupCancel=false;
static uint8_t sensorSetupMode=0;
static WilcoxonILA210RTU::Stats sensorRtuSnapshot;
static wilcoxonila210::TcpStats sensorTcpSnapshot;
static uint8_t sensorTcpClients=0;
static uint32_t sensorSetupTested=0,sensorSetupTotal=0,sensorFoundBaud=0;
static uint8_t sensorFoundId=0;
static bool sensorSetupFound=false;
static const char* sensorSetupMessage="Ready";

void handleSensorSetup() {
    if(server.method()==HTTP_POST) {
        StaticJsonDocument<256> body;
        if(server.arg("plain").length()>256 || deserializeJson(body,server.arg("plain"))) {
            server.send(400,"text/plain","Invalid JSON");return;
        }
        const char* action=body["action"] | "";
        SensorSetupRequest request;
        if(!strcmp(action,"search")) {
            request.mode=1;
            if(!body["first_id"].is<uint8_t>() || !body["last_id"].is<uint8_t>()) {server.send(400,"text/plain","ID range required");return;}
            request.first=body["first_id"];request.last=body["last_id"];
            if(request.first<1 || request.last>247 || request.first>request.last) {server.send(400,"text/plain","Invalid ID range");return;}
        } else if(!strcmp(action,"use") || !strcmp(action,"configure")) {
            request.mode=!strcmp(action,"use")?2:3;
            if(!body["baud"].is<uint32_t>() || !body["id"].is<uint8_t>()) {server.send(400,"text/plain","Baud and ID required");return;}
            request.baud=body["baud"];request.id=body["id"];
            if(wilcoxonila210::baudIndex(request.baud)<0 || request.id<1 || request.id>247) {server.send(400,"text/plain","Unsupported baud or ID");return;}
        } else if(strcmp(action,"cancel")) {server.send(400,"text/plain","Unknown action");return;}
        xSemaphoreTake(dataMutex,portMAX_DELAY);
        if(!strcmp(action,"cancel")) {
            const bool can=sensorSetupBusy && sensorSetupMode==1;
            if(can) sensorSetupCancel=true;
            xSemaphoreGive(dataMutex);server.send(can?202:409,"text/plain",can?"Cancellation queued":"No search to cancel");return;
        }
        const bool busy=sensorSetupBusy || currentDataState!=DD_IDLE || isReadingWaveform || isReadingSpectrum || dynamicStreamActive;
        const bool invalid=request.mode==3 && ((request.baud!=uint32_t(modbus_baud_rate_val))==(request.id!=modbus_slave_id_val));
        if(busy || invalid) {xSemaphoreGive(dataMutex);server.send(busy?409:400,"text/plain",busy?"Capture or setup busy":"Change exactly one setting at a time");return;}
        sensorSetupBusy=true;sensorSetupQueued=true;sensorSetupMode=request.mode;sensorSetupRequest=request;
        sensorSetupFound=false;sensorSetupTested=0;sensorSetupTotal=0;sensorSetupMessage="Queued";
        xSemaphoreGive(dataMutex);server.send(202,"text/plain","Operation queued");return;
    }
    StaticJsonDocument<1536> doc;
    xSemaphoreTake(dataMutex,portMAX_DELAY);
    doc["can_cancel"]=sensorSetupBusy && sensorSetupMode==1;
    doc["busy"]=sensorSetupBusy;doc["message"]=sensorSetupMessage;
    doc["tested"]=sensorSetupTested;doc["total"]=sensorSetupTotal;
    doc["found"]=sensorSetupFound;doc["found_baud"]=sensorFoundBaud;doc["found_id"]=sensorFoundId;
    doc["baud"]=modbus_baud_rate_val;doc["id"]=modbus_slave_id_val;
    doc["read_successes"]=sensorRtuSnapshot.successes;doc["timeouts"]=sensorRtuSnapshot.timeouts;
    doc["crc_errors"]=sensorRtuSnapshot.crcErrors;doc["exceptions"]=sensorRtuSnapshot.exceptions;
    doc["other_errors"]=sensorRtuSnapshot.otherErrors;
    doc["sample_seen"]=sensorHealth.seen;
    if(sensorHealth.seen) doc["reading_age_ms"]=uint32_t(millis()-sensorHealth.lastSuccessAt);
    else doc["reading_age_ms"]=nullptr;
    doc["tcp_clients"]=sensorTcpClients;doc["tcp_requests"]=sensorTcpSnapshot.requests;
    doc["tcp_malformed"]=sensorTcpSnapshot.malformed;doc["tcp_timeouts"]=sensorTcpSnapshot.timeouts;
    xSemaphoreGive(dataMutex);
    doc["mqtt_connected"]=mqttConnected.load();doc["format"]="8N1";
    String response;serializeJson(doc,response);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",response);
}

bool serviceSensorSetup() {
    SensorSetupRequest request;bool queued,cancel;
    xSemaphoreTake(dataMutex,portMAX_DELAY);
    queued=sensorSetupQueued;cancel=sensorSetupCancel;request=sensorSetupRequest;
    sensorSetupQueued=sensorSetupCancel=false;
    const uint32_t baud=modbus_baud_rate_val;const uint8_t id=modbus_slave_id_val;
    xSemaphoreGive(dataMutex);
    if(queued && !sensorSetupController.start(request.mode,baud,id,request.baud,request.id,request.first,request.last)) {
        xSemaphoreTake(dataMutex,portMAX_DELAY);sensorSetupBusy=false;sensorSetupMessage="Invalid setup request";xSemaphoreGive(dataMutex);
    }
    const bool used=sensorSetupController.active;
    if(cancel) sensorSetupController.cancel();
    sensorSetupController.tick(millis());
    if(used && !sensorSetupController.active) node.setTimeout(2000);
    xSemaphoreTake(dataMutex,portMAX_DELAY);
    // Do not clear a new HTTP reservation that arrived during an ordinary poll.
    if(used || queued) {
        sensorSetupBusy=sensorSetupController.active;
        sensorSetupMessage=sensorSetupController.messageText;
        sensorSetupFound=sensorSetupController.found;sensorFoundBaud=sensorSetupController.foundBaud;sensorFoundId=sensorSetupController.foundId;
        sensorSetupTested=sensorSetupController.tested;sensorSetupTotal=sensorSetupController.total;
    }
    sensorRtuSnapshot=node.stats();sensorTcpSnapshot=modbusTCPServer.stats();sensorTcpClients=modbusTCPServer.clientCount();
    const bool reserved=sensorSetupBusy;
    xSemaphoreGive(dataMutex);
    return used || reserved;
}
