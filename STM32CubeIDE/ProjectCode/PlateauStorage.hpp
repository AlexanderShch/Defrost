#ifndef PLATEAU_STORAGE_HPP
#define PLATEAU_STORAGE_HPP
#include "PlateauControl.hpp"
#include <stddef.h>
namespace Plateau {
/* One CRC-protected record; legacy process/sensor records retain their addresses. */
#pragma pack(push,1)
struct StorageRecord {
 uint32_t magic;
 uint16_t version, length;
 PlateauParams params;
 uint16_t crc;
};
#pragma pack(pop)
static_assert(sizeof(StorageRecord)==90,"Plateau EEPROM record size");
inline uint16_t StorageCrc(const StorageRecord& r) {
 const uint8_t* bytes=(const uint8_t*)&r;uint16_t crc=0xffff;
 for(unsigned i=0;i<offsetof(StorageRecord,crc);++i) {
  crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc&1)?(uint16_t)((crc>>1)^0xa001):(uint16_t)(crc>>1);
 }
 return crc;
}
inline bool StorageRecordValid(const StorageRecord& r) {
 return r.magic==0x37444950u && r.version==2 && r.length==sizeof(PlateauParams) &&
  r.crc==StorageCrc(r) && Valid(r.params);
}
/* Backend bool Read/Write(address, bytes, length). Caller owns transaction lock. */
template<class Backend,uint16_t Base=0x180,uint16_t ReservedBytes=128> class Storage {
public:
 static_assert(sizeof(StorageRecord)<=ReservedBytes,"EEPROM record area too short");
 static bool Load(PlateauParams& params) {
  StorageRecord rec={};
  if(!Backend::Read(Base,(uint8_t*)&rec,sizeof(rec))||!StorageRecordValid(rec))return false;
  params=rec.params;return true;
 }
 static bool Save(const PlateauParams& params) {
  if(!Valid(params))return false;
  StorageRecord current={};
  if(!Backend::Read(Base,(uint8_t*)&current,sizeof(current)))return false;
  if(StorageRecordValid(current)&&memcmp(&current.params,&params,sizeof(params))==0)return true;
  StorageRecord next={};next.magic=0x37444950u;next.version=2;next.length=sizeof(params);
  next.params=params;next.crc=StorageCrc(next);
  if(!Backend::Write(Base,(const uint8_t*)&next,sizeof(next)))return false;
  StorageRecord verified={};
  return Backend::Read(Base,(uint8_t*)&verified,sizeof(verified))&&StorageRecordValid(verified)&&
   memcmp(&verified.params,&params,sizeof(params))==0;
 }
};
}
#endif
