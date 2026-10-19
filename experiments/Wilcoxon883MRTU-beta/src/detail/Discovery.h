#pragma once
#include "Identity.h"
namespace wilcoxon883m {
enum class SerialMode : uint8_t { Any, N81, N82, E81, O81, E82, O82 };
struct Settings { uint32_t baud=0; uint8_t unit=0; SerialMode mode=SerialMode::Any; };
inline uint32_t baudForCode(uint16_t code) {
    static const uint32_t rates[]={1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,921600};
    return code>=6 && code<=16 ? rates[code-6] : 0;
}
inline bool validBaud(uint32_t baud) {
    for(uint16_t code=6;code<=16;++code)if(baudForCode(code)==baud)return true;
    return false;
}
inline uint8_t characterBits(SerialMode mode) {
    return mode==SerialMode::N81 ? 10 : (mode==SerialMode::E82 || mode==SerialMode::O82 ? 12 : 11);
}
inline bool reportedSettings(const uint16_t* words,Settings& result) {
    if(!baudForCode(words[0]) || words[1]!=8 || words[2]>2 ||
       words[3]<1 || words[3]>2 || words[4]<1 || words[4]>247)return false;
    static const SerialMode modes[3][2]={{SerialMode::N81,SerialMode::N82},
        {SerialMode::E81,SerialMode::E82},{SerialMode::O81,SerialMode::O82}};
    result.baud=baudForCode(words[0]);result.unit=uint8_t(words[4]);result.mode=modes[words[2]][words[3]-1];return true;
}
inline bool sameSettings(const Settings& a,const Settings& b) {
    return a.baud==b.baud && a.unit==b.unit && a.mode==b.mode;
}
struct DiscoveryOptions {
    uint8_t firstUnit=1,lastUnit=247;
    uint32_t baud=0; // Zero searches all manual-listed rates.
    SerialMode mode=SerialMode::Any;
    uint32_t responseTimeoutMs=250,maxDurationMs=120000;
    bool requireIdentity=true;
};
enum class DiscoveryState { Idle,Searching,Found,Exhausted,TimedOut,Cancelled };
struct DiscoveryResult { Settings settings;Identity identity;bool identityVerified=false; };
struct DiscoveryProgress {
    DiscoveryState state=DiscoveryState::Idle;
    uint32_t attempts=0,totalCandidates=0;
    Settings candidate;
};
// Allow the response to finish on the wire, even at 1200 baud with two
// stop bits. Discovery's configured timeout is a floor, not a fixed budget
// that would incorrectly reject long device-identification frames.
inline uint32_t discoveryTimeout(const Settings& settings,size_t responseBytes,uint32_t floorMs) {
    uint32_t bits=characterBits(settings.mode);
    uint32_t wireMs=(uint32_t(responseBytes)*bits*1000+settings.baud-1)/settings.baud;
    uint32_t silenceMs=settings.baud>19200?2:(3500*bits+settings.baud-1)/settings.baud;
    uint32_t required=wireMs+silenceMs+50;
    return required>floorMs?required:floorMs;
}
// Adapter exposes configure(Settings), readHolding(...) and readIdentity(...).
// Always read settings twice, using the reported format for confirmation. A
// compatible stop-bit probe is not treated as proof of the actual stop bits.
template<class Adapter> bool probeSensor(Adapter& port,const Settings& candidate,
        const DiscoveryOptions& options,DiscoveryResult& output) {
    if(!port.configure(candidate))return false;
    uint16_t words[5];
    if(port.readHolding(0,5,words,5,discoveryTimeout(candidate,15,options.responseTimeoutMs))!=Result::Ok)return false;
    Settings actual;
    if(!reportedSettings(words,actual) || actual.baud!=candidate.baud || actual.unit!=candidate.unit)return false;
    if(!port.configure(actual))return false;
    if(port.readHolding(0,5,words,5,discoveryTimeout(actual,15,options.responseTimeoutMs))!=Result::Ok)return false;
    Settings confirmed;
    if(!reportedSettings(words,confirmed) || !sameSettings(actual,confirmed))return false;
    Identity identity;
    Result status=port.readIdentity(identity,discoveryTimeout(actual,256,options.responseTimeoutMs));
    bool verified=status==Result::Ok && is883M(identity);
    if(status==Result::Ok && !verified)return false;
    if(options.requireIdentity && !verified)return false;
    DiscoveryResult next;next.settings=actual;next.identity=identity;next.identityVerified=verified;
    output=next;return true;
}
// Probe interface: nowMs() and probe(candidate,options,result). Each poll does
// at most one bounded candidate probe. No hidden tasks, writes or allocations.
template<class Probe> class Discovery {
    Probe& probe_;
    DiscoveryOptions options_;
    DiscoveryProgress progress_;
    DiscoveryResult result_;
    uint32_t started_=0;
public:
    explicit Discovery(Probe& probe):probe_(probe){}
    static bool valid(const DiscoveryOptions& o) {
        return o.firstUnit>=1 && o.lastUnit<=247 && o.firstUnit<=o.lastUnit &&
          (!o.baud || validBaud(o.baud)) && unsigned(o.mode)<=unsigned(SerialMode::O82) &&
          o.responseTimeoutMs>=150 && o.responseTimeoutMs<=5000 && o.maxDurationMs<=86400000UL;
    }
    bool start(const DiscoveryOptions& options) {
        if(!valid(options))return false;
        options_=options;progress_=DiscoveryProgress();result_=DiscoveryResult();
        progress_.state=DiscoveryState::Searching;
        progress_.totalCandidates=uint32_t(options.lastUnit-options.firstUnit+1)*(options.baud?1:11)*(options.mode==SerialMode::Any?6:1);
        started_=probe_.nowMs();return true;
    }
    void cancel(){if(progress_.state==DiscoveryState::Searching)progress_.state=DiscoveryState::Cancelled;}
    const DiscoveryProgress& progress()const{return progress_;}
    const DiscoveryResult& result()const{return result_;}
    DiscoveryState poll() {
        if(progress_.state!=DiscoveryState::Searching)return progress_.state;
        if(options_.maxDurationMs && uint32_t(probe_.nowMs()-started_)>=options_.maxDurationMs)
            return progress_.state=DiscoveryState::TimedOut;
        // First try all profiles for the first ID, then each profile across
        // remaining IDs. Try common rates and 8N1 first; never duplicate a tuple.
        static const uint32_t rates[]={9600,115200,19200,38400,57600,1200,2400,4800,230400,460800,921600};
        uint32_t index=progress_.attempts;
        const uint32_t modeCount=options_.mode==SerialMode::Any?6:1,rateCount=options_.baud?1:11;
        const uint32_t profiles=modeCount*rateCount;
        uint32_t profile=index,unit=options_.firstUnit;
        if(index>=profiles){
            index-=profiles;
            const uint32_t remaining=options_.lastUnit-options_.firstUnit;
            profile=index/remaining;unit=options_.firstUnit+1+index%remaining;
        }
        Settings candidate;
        candidate.unit=uint8_t(unit);
        candidate.baud=options_.baud?options_.baud:rates[profile%rateCount];
        candidate.mode=options_.mode==SerialMode::Any?SerialMode(1+profile/rateCount):options_.mode;
        progress_.candidate=candidate;++progress_.attempts;
        DiscoveryResult found;
        bool success=probe_.probe(candidate,options_,found);
        if(options_.maxDurationMs && uint32_t(probe_.nowMs()-started_)>=options_.maxDurationMs)
            return progress_.state=DiscoveryState::TimedOut;
        if(success){result_=found;return progress_.state=DiscoveryState::Found;}
        if(progress_.attempts>=progress_.totalCandidates)progress_.state=DiscoveryState::Exhausted;
        return progress_.state;
    }
};
} // namespace wilcoxon883m
