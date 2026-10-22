#include "WilcoxonILA210RTU.h"
#include "detail/HoldingCache.h"
#include <assert.h>
#include <stdio.h>
using namespace wilcoxonila210;
struct Fake {
    uint64_t clock = 0, responseAt = 0;
    uint8_t reply[300] = {}, request[8] = {};
    size_t length = 0, index = 0;
    bool enabled = false, flushed = false;
    unsigned writes = 0;
    enum Mode { Normal, NoReply, BadCrc, WrongUnit, Exception, Partial, TooLong, WriteFail } mode = Normal;
    int failAddress = -1;
    uint16_t sample = 0x8000;
    bool fixedSample = false;
    uint32_t ms() { return uint32_t(clock / 1000); }
    uint32_t us() { return uint32_t(clock); }
    void idle() { clock += 100; }
    int available() { return !enabled && clock >= responseAt && index < length; }
    int read() { if (!available()) return -1; clock += 10; return reply[index++]; }
    void seal() { auto crc = crc16(reply,length-2);reply[length-2]=crc&255;reply[length-1]=crc>>8; }
    void tx(bool active) { if (!active) assert(flushed); enabled = active; }
    void flush() { assert(enabled); flushed = true; responseAt = clock + 2000; }
    size_t write(const uint8_t* bytes, size_t size) {
        assert(enabled && size == 8 && validCrc(bytes, size));
        ++writes; memcpy(request,bytes,8); index = length = 0; flushed = false;
        if (mode == WriteFail) return 0;
        if (mode == NoReply) return size;
        reply[0] = bytes[0]; reply[1] = bytes[1];
        if (mode == Exception || word(bytes+2) == failAddress) {
            reply[1] |= 0x80;reply[2]=2;length=5;
        } else if (bytes[1] == 6) {
            memcpy(reply,bytes,8);length=8;
        } else {
            unsigned count=word(bytes+4);reply[2]=2*count;length=5+2*count;
            for(unsigned i=0;i<count;++i)put(reply+3+2*i,fixedSample?sample:(word(bytes+2)==0x0050?0xff85:uint16_t(0x8000+i)));
        }
        if (mode == WrongUnit) ++reply[0];
        seal();
        if (mode == BadCrc) reply[length-1] ^= 1;
        if (mode == Partial) length -= 1;
        if (mode == TooLong) { length=257;memset(reply,1,length); }
        return size;
    }
};

Fake io;
uint32_t millis() { return io.ms(); }
uint32_t micros() { return io.us(); }
void yield() {}
void delayMicroseconds(uint32_t amount) { io.clock += amount; }
int HardwareSerial::available() { return io.available(); }
int HardwareSerial::read() { return io.read(); }
size_t HardwareSerial::write(const uint8_t* data, size_t count) { return io.write(data,count); }
void HardwareSerial::flush(bool txOnly) { assert(txOnly); io.flush(); }
void before() { io.tx(true); }
void after() { io.tx(false); }
int main() {
 HardwareSerial serial; WilcoxonILA210RTU node;
 assert(node.readInputRegisters(0,1)==0xe4);
 assert(!node.begin(0,serial,115200)); assert(node.begin(1,serial,115200));
 node.preTransmission(before);node.postTransmission(after);
 assert(node.readInputRegisters(0,0)==0xe4);assert(node.readInputRegisters(0,65)==0xe4);assert(io.writes==0);
 assert(node.readInputRegisters(0,64)==0);assert(node.getResponseBuffer(63)==0x803f);assert(node.getResponseBuffer(64)==0);
 io.mode=Fake::BadCrc;assert(node.readInputRegisters(0,1)==0xe3);assert(node.getResponseBuffer(0)==0);assert(!io.enabled);
 io.mode=Fake::Exception;assert(node.readInputRegisters(0,1)==2);assert(node.lastResult()==Result::Exception);
 io.mode=Fake::NoReply;assert(node.readInputRegisters(0,1)==0xe2);assert(!io.enabled);
 io.mode=Fake::Normal;assert(node.readHoldingRegisters(0x0200,1)==0);assert(io.request[1]==3);
 assert(node.writeSingleRegister(0x0200,0xffff)==0);assert(io.request[1]==6&&word(io.request+4)==0xffff);
 assert(node.getResponseBuffer(0)==0);assert(node.readInputRegisters(65535,2)==0xe4);
 float metric=123;assert(node.readMetric(wilcoxonila210::MetricCount,metric)==0xe4&&metric==123);assert(node.exceptionCode()==0);
 assert(node.readMetric(11,metric)==0&&metric > 6541.29f&&metric < 6541.31f);assert(word(io.request+2)==0x0050);
 io.mode=Fake::BadCrc;metric=321;assert(node.readMetric(5,metric)==0xe3&&metric==321);
 io.mode=Fake::Normal;assert(node.readMetric(5,metric)==0&&metric > 3276.79f&&metric < 3276.81f);assert(word(io.request+2)==0x0024);
 assert(node.readMetric(0,metric)==0&&metric>327.67f&&metric<327.69f);
 node.setTimeout(250);io.mode=Fake::NoReply;
 const auto beforeTimeouts=node.stats().timeouts;const auto started=io.clock;
 assert(node.readInputRegisters(0,1)==0xe2);
 assert(io.clock-started>=250000&&io.clock-started<300000);
 assert(node.stats().timeouts==beforeTimeouts+1&&node.stats().crcErrors>0&&node.stats().exceptions>0);
 assert(node.stats().successes>0);
 node.setTimeout(2000);io.mode=Fake::Normal;
 const uint16_t expected[]={0x0000,0x0004,0x0010,0x0014,0x0020,0x0024,0x0030,0x0034,0x0040,0x0044,0x0048,0x0050,0x0060,0x007c};
 for(unsigned i=0;i<wilcoxonila210::MetricCount;i++)assert(wilcoxonila210::MetricRegisters[i].inputAddress==expected[i]);
 // Manual pp.18-19: every published static metric is unsigned.
 io.fixedSample=true;io.sample=0xffff;
 const float scales[]={.01f,.01f,.01f,.01f,.1f,.1f,1,1,.01f,.01f,.01f,.1f,.01f,.1f};
 for(unsigned i=0;i<MetricCount;i++) {
  assert(node.readMetric(i,metric)==0);
  const float expectedValue=65535.0f*scales[i];
  assert(metric>=expectedValue-.01f&&metric<=expectedValue+.01f);
  assert(word(io.request+2)==expected[i]&&io.request[1]==4);
 }
 assert(SpectrumStart+SpectrumPoints-1==0x1f00);
 assert(WaveformStart+WaveformPoints-1==0x8300);
 HoldingCache bank;uint16_t out[50]={},values[2]={11,22};
 assert(bank.readRegisters(false,0,1,out)==IllegalAddress);assert(bank.add(0,0,50));assert(!bank.add(0,5,1));
 assert(bank.set(13,0xff85));assert(bank.readRegisters(false,13,1,out)==0&&out[0]==0xff85);
 assert(bank.readRegisters(true,0,1,out)==IllegalAddress);assert(bank.readRegisters(false,49,2,out)==IllegalAddress);
 assert(bank.writeRegisters(49,2,values)==IllegalAddress);assert(bank.readRegisters(false,49,1,out)==0&&out[0]==0);
 assert(bank.writeRegisters(48,2,values)==0);assert(bank.readRegisters(false,48,2,out)==0&&out[0]==11&&out[1]==22);
 uint8_t req[]={3,0,48,0,2},reply[260]={};assert(servePdu(req,sizeof(req),reply,bank)==6&&reply[0]==3&&word(reply+2)==11&&word(reply+4)==22);
 uint8_t invalid[]={16,0,49,0,2,4,0,1,0,2};assert(servePdu(invalid,sizeof(invalid),reply,bank)==2&&reply[1]==IllegalAddress);
 puts("PASS ILA RTU adapter: 64-register bounds, validated-buffer lifetime, CRC, timeout, exception, direction/flush and FC03/06; TCP 50-register holding cache and atomic bounds.");
}
