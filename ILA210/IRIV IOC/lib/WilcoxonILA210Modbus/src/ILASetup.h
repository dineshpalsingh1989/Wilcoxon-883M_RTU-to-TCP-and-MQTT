#pragma once
#include <stdint.h>

namespace wilcoxonila210 {
static const uint32_t SetupBauds[] = {9600, 19200, 38400, 57600, 115200};
inline int baudIndex(uint32_t baud) {
    for (int i=0;i<5;++i) if (SetupBauds[i]==baud) return i;
    return -1;
}
// Single background owner. Link supplies connect, probe, write and save.
// Each tick performs at most one bounded RTU exchange.
template<class Link> class SetupController {
    Link& link_;
    uint8_t mode_=0, phase_=0, rate_=0, first_=1, last_=16, candidate_=1, tries_=0;
    uint8_t oldId_=1, targetId_=1;
    uint32_t oldBaud_=9600, targetBaud_=9600, due_=0;
    void finish(const char* message) { active=false; messageText=message; }
    void restore() { link_.connect(oldBaud_,oldId_); }
    void accept() {
        const bool saved=link_.save(targetBaud_,targetId_);
        finish(saved ? "Connection verified and saved." : "Connection verified; storage failed. Settings are active until restart.");
    }
public:
    bool active=false, found=false;
    uint32_t foundBaud=0, tested=0, total=0;
    uint8_t foundId=0;
    const char* messageText="Ready";
    explicit SetupController(Link& link):link_(link) {}
    bool start(uint8_t mode,uint32_t currentBaud,uint8_t currentId,
               uint32_t baud,uint8_t id,uint8_t first,uint8_t last) {
        if(active || mode<1 || mode>3 || baudIndex(currentBaud)<0 || currentId<1 || currentId>247) return false;
        if(mode==1 ? (first<1 || last>247 || first>last) : (baudIndex(baud)<0 || id<1 || id>247)) return false;
        // Change one setting at a time so recovery needs only two candidates.
        if(mode==3 && ((baud!=currentBaud)==(id!=currentId))) return false;
        oldBaud_=currentBaud;oldId_=currentId;targetBaud_=baud;targetId_=id;
        mode_=mode;phase_=rate_=tries_=0;first_=first;last_=last;candidate_=first;
        active=true;found=false;tested=0;total=mode==1?5u*(last-first+1):0;
        messageText=mode==1?"Searching 8N1 connections...":"Verifying sensor settings...";
        return true;
    }
    void cancel() { if(active && mode_==1) {restore();finish("Search cancelled; original connection restored.");} }
    void tick(uint32_t now) {
        if(!active) return;
        if(mode_==1) {
            link_.connect(SetupBauds[rate_],candidate_);++tested;
            if(link_.probe(SetupBauds[rate_],candidate_)) {
                found=true;foundBaud=SetupBauds[rate_];foundId=candidate_;
                restore();finish("Compatible sensor found. Select Use connection to verify and save.");return;
            }
            if(candidate_==last_) {candidate_=first_;++rate_;} else ++candidate_;
            if(rate_==5) {restore();finish("No compatible sensor found; original connection restored.");}
            return;
        }
        if(mode_==2) {
            link_.connect(targetBaud_,targetId_);
            if(link_.probe(targetBaud_,targetId_)) accept();
            else {restore();finish("Connection verification failed; original connection restored.");}
            return;
        }
        if(phase_==0) {
            link_.connect(oldBaud_,oldId_);
            if(!link_.probe(oldBaud_,oldId_)) {restore();finish("Current connection does not verify; sensor was not changed.");return;}
            phase_=1;return;
        }
        if(phase_==1) {
            // An ID/baud write may take effect before its acknowledgement.
            // Readback at the new connection decides success, not the echo.
            link_.write(targetId_!=oldId_?4:0,targetId_!=oldId_?targetId_:uint16_t(baudIndex(targetBaud_)+9));
            link_.connect(targetBaud_,targetId_);due_=now+500;phase_=2;return;
        }
        if(int32_t(now-due_)<0) return;
        if(link_.probe(targetBaud_,targetId_)) {accept();return;}
        if(++tries_>=3) {restore();finish("New connection not verified. Sensor may have changed or require restart; search before retrying. Gateway restored.");}
        else due_=now+500;
    }
};
}
