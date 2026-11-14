#include "detail/Discovery.h"
#include "detail/RtuClient.h"
#include <assert.h>
#include <stdio.h>
using namespace wilcoxon883m;
static Settings settings(uint32_t baud,uint8_t unit,SerialMode mode){Settings s;s.baud=baud;s.unit=unit;s.mode=mode;return s;}
static void seal(uint8_t* frame,size_t size){auto c=crc16(frame,size-2);frame[size-2]=uint8_t(c);frame[size-1]=uint8_t(c>>8);}
static size_t identityFrame(uint8_t* p,uint8_t code,uint8_t id,const char* text){
    size_t n=strlen(text);const uint8_t header[]={1,0x2b,0x0e,code,2,0,0,1};memcpy(p,header,8);
    p[8]=id;p[9]=uint8_t(n);memcpy(p+10,text,n);seal(p,n+12);return n+12;
}
struct IdentityIo {
    uint64_t clock=0,at=0;uint8_t reply[256]={};size_t count=0,index=0;
    bool txEnabled=false,unsupported=false,badCrc=false,paginate=false;
    uint32_t ms(){return uint32_t(clock/1000);}uint32_t us(){return uint32_t(clock);}
    void idle(){clock+=100;}
    int available(){return !txEnabled && clock>=at && index<count;}
    int read(){return available()?reply[index++]:-1;}
    void tx(bool enabled){txEnabled=enabled;}
    void flush(){at=clock+1000;}
    size_t write(const uint8_t* request,size_t size){
        assert(txEnabled&&size==7&&validCrc(request,size)&&request[1]==0x2b&&request[2]==0x0e);
        index=0;uint8_t code=request[3];
        if(unsupported&&code==2){reply[0]=1;reply[1]=0xab;reply[2]=1;count=5;seal(reply,count);return size;}
        if(code==1)count=identityFrame(reply,code,0,"Wilcoxon");
        else if(paginate&&request[4]==3){count=identityFrame(reply,code,3,"www.wilcoxon.com");reply[5]=0xff;reply[6]=5;seal(reply,count);}
        else count=identityFrame(reply,code,5,"883M");
        if(badCrc)reply[count-1]^=1;
        return size;
    }
};
struct Adapter {
    Settings actual=settings(115200,7,SerialMode::E82),configured;
    bool mismatch=false,wrongModel=false,unsupported=false;
    unsigned reads=0,writes=0;uint32_t identityTimeout=0;
    bool configure(const Settings& s){configured=s;return true;}
    Result readHolding(uint16_t address,uint16_t count,uint16_t* out,size_t capacity,uint32_t){
        assert(address==0&&count==5&&capacity>=5);++reads;
        uint16_t code=6;while(baudForCode(code)!=actual.baud)++code;
        out[0]=code;out[1]=8;out[2]=1;out[3]=2;out[4]=mismatch&&reads>1?8:actual.unit;
        return Result::Ok;
    }
    Result readIdentity(Identity& out,uint32_t timeout){
        identityTimeout=timeout;
        if(unsupported)return Result::Exception;
        strcpy(out.vendor,"Wilcoxon");strcpy(out.model,wrongModel?"Other":"883M");return Result::Ok;
    }
};
struct Probe {
    uint32_t clock=0,calls=0;bool find=false;
    bool seen[247][66]={};
    uint32_t nowMs(){return clock;}
    bool probe(const Settings& s,const DiscoveryOptions&,DiscoveryResult& out){
        uint16_t code=6;while(baudForCode(code)!=s.baud){++code;assert(code<=16);}
        unsigned profile=(unsigned(s.mode)-1)*11+code-6;
        assert(!seen[s.unit-1][profile]);seen[s.unit-1][profile]=true;
        ++calls;clock+=10;
        if(find){out.settings=s;strcpy(out.identity.vendor,"Wilcoxon");strcpy(out.identity.model,"883M");out.identityVerified=true;return true;}
        return false;
    }
};
int main(){
    uint8_t request[7];identityRequest(1,1,0,request);
    const uint8_t expected[]={1,0x2b,0x0e,1,0,0x70,0x77};assert(!memcmp(request,expected,7));
    uint8_t frame[256],exception=0;Identity identity;IdentityPage page;
    size_t n=identityFrame(frame,1,0,"Wilcoxon");
    assert(parseIdentity(frame,n,1,1,0,identity,page,exception)==Result::Ok);
    assert(!strcmp(identity.vendor,"Wilcoxon"));
    frame[n-1]^=1;assert(parseIdentity(frame,n,1,1,0,identity,page,exception)==Result::Crc);frame[n-1]^=1;
    frame[9]=250;seal(frame,n);assert(parseIdentity(frame,n,1,1,0,identity,page,exception)==Result::Frame);
    n=identityFrame(frame,2,5,"883M");frame[5]=0xff;frame[6]=5;seal(frame,n);
    assert(parseIdentity(frame,n,1,2,3,identity,page,exception)==Result::Frame);
    n=identityFrame(frame,2,5,"883M");frame[10]=0;seal(frame,n);
    assert(parseIdentity(frame,n,1,2,3,identity,page,exception)==Result::Frame);
    const char* longName="883M triaxial vibration and temperature sensor, Modbus RTU";
    n=identityFrame(frame,2,4,longName);
    assert(parseIdentity(frame,n,1,2,3,identity,page,exception)==Result::Ok);
    assert(strlen(identity.productName)==sizeof(identity.productName)-1&&!strncmp(identity.productName,longName,39));
    n=identityFrame(frame,1,1,"0123456789012345678901234567890123456789");
    assert(parseIdentity(frame,n,1,1,0,identity,page,exception)==Result::Frame); // Required fields stay strict.
    puts("PASS manual identity request vector, CRC, bounded objects, text, pagination and optional-object truncation");

    IdentityIo io;Client<IdentityIo> client(io);assert(client.configure(9600,1,10));
    assert(client.readIdentity(identity,100)==Result::Ok&&is883M(identity));
    io.paginate=true;assert(client.readIdentity(identity,100)==Result::Ok&&is883M(identity));
    assert(!strcmp(identity.vendorUrl,"www.wilcoxon.com"));
    Identity saved=identity;io.unsupported=true;
    assert(client.readIdentity(identity,100)==Result::Exception&&client.exceptionCode()==1);
    assert(!strcmp(identity.model,saved.model));io.unsupported=false;io.badCrc=true;
    assert(client.readIdentity(identity,100)==Result::Crc);
    puts("PASS complete and paginated identity transport, unsupported firmware and preserved output on failure");

    uint16_t words[]={13,8,1,2,7};Settings decoded;
    assert(reportedSettings(words,decoded)&&decoded.mode==SerialMode::E82&&decoded.unit==7&&decoded.baud==115200);
    words[0]=17;assert(!reportedSettings(words,decoded));words[0]=13;
    words[4]=0;assert(!reportedSettings(words,decoded));words[4]=7;
    words[1]=7;assert(!reportedSettings(words,decoded));
    Adapter adapter;DiscoveryOptions options;DiscoveryResult result;
    // Actual stop bits differ from the initial compatible probe.
    assert(probeSensor(adapter,settings(115200,7,SerialMode::E81),options,result));
    assert(result.settings.mode==SerialMode::E82&&result.identityVerified&&adapter.reads==2&&adapter.writes==0);
    adapter.reads=0;adapter.mismatch=true;assert(!probeSensor(adapter,adapter.actual,options,result));
    adapter.mismatch=false;adapter.wrongModel=true;assert(!probeSensor(adapter,adapter.actual,options,result));
    adapter.wrongModel=false;adapter.unsupported=true;assert(!probeSensor(adapter,adapter.actual,options,result));
    options.requireIdentity=false;assert(probeSensor(adapter,adapter.actual,options,result)&&!result.identityVerified);
    adapter.unsupported=false;adapter.wrongModel=true;assert(!probeSensor(adapter,adapter.actual,options,result));
    adapter.wrongModel=false;adapter.actual.baud=1200;
    assert(probeSensor(adapter,adapter.actual,options,result)&&adapter.identityTimeout>=2645);
    assert(discoveryTimeout(settings(9600,1,SerialMode::N81),256,250)>=321);
    assert(discoveryTimeout(settings(115200,1,SerialMode::N81),256,250)==250);
    puts("PASS low-baud identity response timeouts include maximum frame wire time");
    puts("PASS reported-settings confirmation, parity/stop-bit aliases, wrong-model rejection and explicit legacy fallback");

    Probe probe;Discovery<Probe> scanner(probe);options=DiscoveryOptions();options.maxDurationMs=0;
    assert(scanner.start(options));
    while(scanner.poll()==DiscoveryState::Searching){}
    assert(scanner.progress().state==DiscoveryState::Exhausted&&probe.calls==16302);
    assert(scanner.progress().attempts==scanner.progress().totalCandidates);
    Probe narrowed;Discovery<Probe> narrow(narrowed);options.firstUnit=7;options.lastUnit=7;options.baud=115200;options.mode=SerialMode::E82;
    assert(narrow.start(options));assert(narrow.poll()==DiscoveryState::Exhausted&&narrowed.calls==1);
    Probe found;found.find=true;Discovery<Probe> finder(found);
    assert(finder.start(options));assert(finder.poll()==DiscoveryState::Found&&finder.result().identityVerified);
    assert(finder.poll()==DiscoveryState::Found&&found.calls==1);
    Probe cancel;Discovery<Probe> canceller(cancel);assert(canceller.start(options));canceller.cancel();
    assert(canceller.poll()==DiscoveryState::Cancelled&&cancel.calls==0);
    Probe clock;clock.clock=0xfffffffa;Discovery<Probe> timer(clock);options=DiscoveryOptions();options.maxDurationMs=15;
    assert(timer.start(options));assert(timer.poll()==DiscoveryState::Searching);
    assert(timer.poll()==DiscoveryState::TimedOut&&clock.calls==2);
    options.firstUnit=0;assert(!timer.start(options));options=DiscoveryOptions();options.responseTimeoutMs=1;assert(!timer.start(options));
    puts("PASS all 16,302 unique scan tuples, narrowed search, success, cancellation, deadline and timer wrap");
}
