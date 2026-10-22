#pragma once
#include "RtuProtocol.h"
namespace wilcoxon883m {
struct Identity {
    char vendor[32] = {}, product[40] = {}, revision[24] = {}, model[24] = {};
    // Optional regular objects 3 and 4; longer values are truncated.
    char vendorUrl[64] = {}, productName[40] = {};
};
struct IdentityPage { bool more = false; uint8_t next = 0; };
inline size_t identityRequest(uint8_t unit, uint8_t code, uint8_t first, uint8_t* out) {
    out[0]=unit;out[1]=0x2b;out[2]=0x0e;out[3]=code;out[4]=first;
    uint16_t crc=crc16(out,5);out[5]=uint8_t(crc);out[6]=uint8_t(crc>>8);return 7;
}
inline bool identityString(char* out,size_t capacity,const uint8_t* value,size_t count,bool truncate=false) {
    if (!count || (count >= capacity && !truncate)) return false;
    for(size_t i=0;i<count;++i)if(value[i]<32 || value[i]>126)return false;
    if (count >= capacity) count = capacity - 1;
    memcpy(out,value,count);out[count]=0;return true;
}
inline Result parseIdentity(const uint8_t* frame,size_t size,uint8_t unit,uint8_t code,
        uint8_t first,Identity& identity,IdentityPage& page,uint8_t& exception) {
    exception=0;
    if(!frame || size<5 || size>256)return Result::Frame;
    if(!validCrc(frame,size))return Result::Crc;
    if(frame[0]!=unit)return Result::Frame;
    if(frame[1]==0xab){if(size!=5 || !frame[2])return Result::Frame;exception=frame[2];return Result::Exception;}
    if(size<10 || frame[1]!=0x2b || frame[2]!=0x0e || frame[3]!=code ||
       (frame[4]&0x7f)<1 || (frame[4]&0x7f)>3 || (frame[5]!=0 && frame[5]!=0xff))return Result::Frame;
    size_t cursor=8;int previous=int(first)-1;
    for(unsigned i=0;i<frame[7];++i){
        if(cursor+2>size-2)return Result::Frame;
        uint8_t id=frame[cursor++],length=frame[cursor++];
        if(id<=previous || cursor+length>size-2)return Result::Frame;
        previous=id;bool ok=true;
        switch(id){
            case 0:ok=identityString(identity.vendor,sizeof(identity.vendor),frame+cursor,length);break;
            case 1:ok=identityString(identity.product,sizeof(identity.product),frame+cursor,length);break;
            case 2:ok=identityString(identity.revision,sizeof(identity.revision),frame+cursor,length);break;
            case 3:ok=identityString(identity.vendorUrl,sizeof(identity.vendorUrl),frame+cursor,length,true);break;
            case 4:ok=identityString(identity.productName,sizeof(identity.productName),frame+cursor,length,true);break;
            case 5:ok=identityString(identity.model,sizeof(identity.model),frame+cursor,length);break;
            default:break;
        }
        if(!ok)return Result::Frame;
        cursor+=length;
    }
    if(cursor!=size-2)return Result::Frame;
    page.more=frame[5]==0xff;page.next=frame[6];
    if(page.more && (!frame[7] || page.next<=previous))return Result::Frame;
    return Result::Ok;
}
inline bool is883M(const Identity& identity) {
    return strcmp(identity.vendor,"Wilcoxon")==0 && strcmp(identity.model,"883M")==0;
}
} // namespace wilcoxon883m
