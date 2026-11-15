#include "PlateauStorage.hpp"
#define CHECK(x) do {if(!(x))return __LINE__;}while(0)
struct Memory {
 static uint8_t bytes[2048];static int remaining,writes;static bool readable,corruptAfterWrite;
 static bool Read(uint16_t address,uint8_t* dst,uint16_t length) {
  if(!readable||address+length>sizeof(bytes))return false;
  memcpy(dst,bytes+address,length);return true;
 }
 static bool Write(uint16_t address,const uint8_t* src,uint16_t length) {
  if(address+length>sizeof(bytes))return false;
  ++writes;
  for(unsigned i=0;i<length;++i){if(remaining==0)return false;if(remaining>0)--remaining;bytes[address+i]=src[i];}
  if(corruptAfterWrite)bytes[address+20]^=1;
  return true;
 }
};
uint8_t Memory::bytes[2048];int Memory::remaining=-1,Memory::writes=0;bool Memory::readable=true;
bool Memory::corruptAfterWrite=false;
typedef Plateau::Storage<Memory> Store;
extern "C" int test_main() {
 memset(Memory::bytes,0x6d,sizeof(Memory::bytes));
 PlateauParams p=Plateau::Defaults(),loaded=p,changed=p;changed.tempKi=.004f;changed.tempKp=.31f;
 CHECK(!Store::Load(loaded));CHECK(memcmp(&loaded,&p,sizeof(p))==0);
 CHECK(Store::Save(changed));
 // Simulate loss of all volatile configuration, then the production boot Load.
 loaded=Plateau::Defaults();CHECK(Store::Load(loaded));CHECK(memcmp(&loaded,&changed,sizeof(p))==0);
 for(unsigned i=0;i<0x180;++i)CHECK(Memory::bytes[i]==0x6d);
 for(unsigned i=0x200;i<2048;++i)CHECK(Memory::bytes[i]==0x6d);
 int writes=Memory::writes;CHECK(Store::Save(changed));CHECK(Memory::writes==writes);
 uint8_t original[2048];memcpy(original,Memory::bytes,2048);
 // Single record: interrupted writes may yield old/new valid data or CRC rejection.
 for(int cut=0;cut<=90;++cut) {
  memcpy(Memory::bytes,original,2048);Memory::remaining=cut;
  bool saved=Store::Save(p);Memory::remaining=-1;loaded=Plateau::Defaults();
  bool valid=Store::Load(loaded);
  CHECK(saved==(cut==90));
  if(valid)CHECK(memcmp(&loaded,&p,sizeof(p))==0||memcmp(&loaded,&changed,sizeof(p))==0);
  else CHECK(memcmp(&loaded,&p,sizeof(p))==0);
  if(saved)CHECK(valid&&memcmp(&loaded,&p,sizeof(p))==0);
 }
 // Corruption anywhere in the record must be rejected; no backup record exists.
 for(unsigned i=0;i<sizeof(Plateau::StorageRecord);++i) {
  memcpy(Memory::bytes,original,2048);Memory::bytes[0x180+i]^=1;
  loaded=p;CHECK(!Store::Load(loaded));CHECK(memcmp(&loaded,&p,sizeof(p))==0);
 }
 memcpy(Memory::bytes,original,2048);Memory::readable=false;writes=Memory::writes;
 CHECK(!Store::Save(p));CHECK(!Store::Load(loaded));CHECK(Memory::writes==writes);Memory::readable=true;
 PlateauParams invalid=p;invalid.tempKi=NAN;CHECK(!Store::Save(invalid));
 memcpy(Memory::bytes,original,2048);Memory::corruptAfterWrite=true;
 CHECK(!Store::Save(p));CHECK(!Store::Load(loaded));Memory::corruptAfterWrite=false;
 Plateau::StorageRecord r={};memcpy(&r,original+0x180,sizeof(r));r.version=99;r.crc=Plateau::StorageCrc(r);
 memcpy(Memory::bytes+0x180,&r,sizeof(r));CHECK(!Store::Load(loaded));
 // Startup fallback is backfilled and remains readable on the next reboot.
 CHECK(Store::Save(p));loaded=changed;CHECK(Store::Load(loaded));CHECK(memcmp(&loaded,&p,sizeof(p))==0);
 return 0;
}
