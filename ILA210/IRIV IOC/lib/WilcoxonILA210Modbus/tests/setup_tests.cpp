#include "ILASetup.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace wilcoxonila210;
struct Link {
 uint32_t baud=115200,connectedBaud=115200;uint8_t id=7,connectedId=7;
 unsigned probes=0,writes=0,saves=0;bool present=true,writeWorks=true,saveWorks=true;
 void connect(uint32_t b,uint8_t i){connectedBaud=b;connectedId=i;}
 bool probe(uint32_t b,uint8_t i){++probes;return present&&b==baud&&i==id&&connectedBaud==b&&connectedId==i;}
 void write(uint16_t address,uint16_t value){++writes;if(writeWorks){if(address==4)id=uint8_t(value);else {assert(address==0&&value>=9&&value<=13);baud=SetupBauds[value-9];}}}
 bool save(uint32_t b,uint8_t i){assert(probe(b,i));++saves;return saveWorks;}
};
int main(){
 Link link;SetupController<Link> setup(link);
 assert(!setup.start(1,9600,1,0,0,0,16));
 assert(!setup.start(3,9600,1,19200,2,1,16));
 assert(!setup.start(3,9600,1,9600,1,1,16));
 assert(setup.start(1,9600,1,0,0,1,16));
 assert(!setup.start(1,9600,1,0,0,1,16));
 while(setup.active)setup.tick(0);
 assert(setup.found&&setup.foundBaud==115200&&setup.foundId==7);
 assert(link.writes==0&&link.saves==0&&link.connectedBaud==9600&&link.connectedId==1);
 assert(setup.start(2,9600,1,115200,7,1,16));setup.tick(0);
 assert(!setup.active&&link.saves==1&&link.writes==0);
 assert(setup.start(3,115200,7,115200,8,1,16));
 setup.tick(0);assert(link.writes==0);setup.tick(10);assert(link.writes==1);
 setup.tick(509);assert(setup.active);setup.tick(510);assert(!setup.active&&link.id==8&&link.saves==2);
 assert(setup.start(3,115200,8,19200,8,1,16));setup.tick(0);setup.tick(10);setup.tick(510);
 assert(!setup.active&&link.baud==19200&&link.saves==3);
 link.writeWorks=false;
 assert(setup.start(3,19200,8,9600,8,1,16));setup.tick(0);setup.tick(10);
 setup.tick(510);setup.tick(1010);setup.tick(1510);
 assert(!setup.active&&link.saves==3&&link.connectedBaud==19200&&strstr(setup.messageText,"may have changed"));
 link.present=false;
 assert(setup.start(1,19200,8,0,0,1,247));
 while(setup.active)setup.tick(0);
 assert(setup.tested==1235&&!setup.found&&link.connectedId==8&&link.connectedBaud==19200);
 assert(setup.start(1,19200,8,0,0,1,247));setup.tick(0);setup.cancel();
 assert(!setup.active&&setup.tested==1&&link.connectedId==8);
 auto writes=link.writes;
 assert(setup.start(3,19200,8,19200,9,1,16));setup.tick(0);
 assert(!setup.active&&link.writes==writes); // Never write to an unverified connection.
 link.present=link.writeWorks=true;link.saveWorks=false;
 assert(setup.start(3,19200,8,19200,9,1,16));setup.tick(0);setup.tick(10);setup.tick(510);
 assert(!setup.active&&strstr(setup.messageText,"storage failed")&&link.connectedId==9);
 assert(setup.start(2,19200,9,9600,1,1,16));setup.tick(0);
 assert(!setup.active&&link.connectedBaud==19200&&link.connectedId==9);
 // Verification delay remains bounded across millis rollover.
 assert(setup.start(3,19200,9,19200,10,1,16));setup.tick(0xfffffff0u);setup.tick(0xfffffff1u);
 setup.tick(100);assert(setup.active);setup.tick(500);assert(!setup.active&&link.id==10);
 puts("PASS setup: bounded search, cancellation/restoration, verified selection, ID/baud changes, failed writes/storage and timer wrap (simulated).");
}
