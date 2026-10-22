#include "detail/RtuClient.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
using namespace wilcoxon883m;
struct Fake {
    uint64_t clock = 0, responseAt = 0;
    uint8_t reply[300] = {}, request[8] = {};
    size_t length = 0, index = 0;
    bool enabled = false, flushed = false;
    unsigned writes = 0;
    enum Mode { Normal, NoReply, BadCrc, WrongUnit, Exception, Partial, TooLong, WriteFail } mode = Normal;
    int failAddress = -1;
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
            for(unsigned i=0;i<count;++i)put(reply+3+2*i,word(bytes+2)==0x0050?0xff85:uint16_t(0x8000+i));
        }
        if (mode == WrongUnit) ++reply[0];
        seal();
        if (mode == BadCrc) reply[length-1] ^= 1;
        if (mode == Partial) length -= 1;
        if (mode == TooLong) { length=257;memset(reply,1,length); }
        return size;
    }
};
int main() {
    const uint8_t known[] = {1,3,0,0,0,10};assert(crc16(known,6)==0xcdc5);
    Fake io;Client<Fake> client(io);uint16_t values[125];values[0]=42;
    assert(client.readInput(0,1,values,125)==Result::InvalidArgument&&io.writes==0);
    assert(!client.configure(0,1,10));assert(!client.configure(9600,0,10));
    assert(client.configure(9600,1,10));
    assert(client.readInput(0,125,values,125)==Result::Ok);
    for(unsigned i=0;i<125;++i)assert(values[i]==0x8000+i);
    assert(!io.enabled&&io.flushed);
    unsigned writes=io.writes;
    assert(client.readInput(65535,2,values,125)==Result::InvalidArgument);
    assert(client.readInput(0,126,values,125)==Result::InvalidArgument);
    assert(client.readInput(0,125,values,124)==Result::InvalidArgument);
    assert(client.readInput(0,1,values,125,0)==Result::InvalidArgument);
    assert(io.writes==writes);
    puts("PASS CRC vector, initialization, 125 registers, buffer/range bounds and direction sequencing");
    const Fake::Mode modes[]={Fake::BadCrc,Fake::WrongUnit,Fake::Exception,Fake::Partial,Fake::TooLong,Fake::NoReply,Fake::WriteFail};
    const Result results[]={Result::Crc,Result::Frame,Result::Exception,Result::Crc,Result::Frame,Result::Timeout,Result::Transport};
    for(unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);++i){
        io.mode=modes[i];values[0]=42;
        assert(client.readInput(0,3,values,125,30)==results[i]);
        assert(values[0]==42&&!io.enabled);
        if(modes[i]==Fake::Exception)assert(client.exceptionCode()==2);
    }
    io.mode=Fake::Normal;
    assert(client.readHolding(0x0200,1,values,125)==Result::Ok);
    assert(io.request[1]==3&&word(io.request+2)==0x0200);
    assert(client.writeSingle(0x0200,0xffff)==Result::Ok);
    assert(io.request[1]==6&&word(io.request+4)==0xffff);
    puts("PASS corrupt/partial/oversized replies, wrong unit, exceptions, timeout and FC03/06");
    Metrics metrics = {};
    assert(readMetrics(client,metrics,100)==Result::Ok);
    assert(fabsf(metrics.accelerationRmsG[0]-327.68f)<0.001f);
    assert(fabsf(metrics.velocityRmsMmS[0]-3276.8f)<0.01f);
    assert(fabsf(metrics.temperatureC+12.3f)<0.001f);
    Metrics saved=metrics;io.failAddress=0x0020;
    assert(readMetrics(client,metrics,100)==Result::Exception);
    assert(metrics.temperatureC==saved.temperatureC&&metrics.velocityRmsMmS[0]==saved.velocityRmsMmS[0]);
    io.failAddress=-1;
    // Test both microsecond and millisecond wrap with monotonic fake time.
    io.clock=uint64_t(0xffffffffUL)-1000;assert(client.readInput(0,1,values,125)==Result::Ok);
    io.clock=uint64_t(0xffffffffUL)*1000-1000;assert(client.readInput(0,1,values,125)==Result::Ok);
    puts("PASS unsigned vibration/signed temperature scaling, failed snapshot preservation and timer wrap");
    puts("All RTU beta library tests passed (simulated transport; no hardware).");
}
