
// Uses the production server Commands.cpp; UART wrapper copied from ModBus.cpp.
#include "Commands.h"
#include "ServerTxSize.h"
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
#define MAX_MB_BUFSIZE SERVER_TX_ITEM_SIZE
#define SYNC_START_1 0xaa
#define SYNC_START_2 0x55
static unsigned sent;
static uint8_t wire[128];
typedef int MB_Error_t;
const int MB_ERROR_WRONG_VALUE=1;
int WriteToServer(uint8_t* p,int n){sent=n;memcpy(wire,p,n);return 0;}
MB_Error_t WriteToServerWithSync(uint8_t* Data, int length)
{
    if (Data == nullptr || length <= 0 || length > MAX_MB_BUFSIZE - 2) return MB_ERROR_WRONG_VALUE;
	static uint8_t TxBufferWithSync[MAX_MB_BUFSIZE];

	TxBufferWithSync[0] = SYNC_START_1;  // 0xAA
	TxBufferWithSync[1] = SYNC_START_2;  // 0x55
	
	// Добавляем в буфер данные (Type + Len + PayLoad + CRC)
	memcpy(&TxBufferWithSync[2], Data, length);
	
	return WriteToServer(TxBufferWithSync, length + 2);	// Отправляем пакет на сервер
}
extern "C" int test_main(){
 uint8_t payload[123]={},buffer[128]={};
 Command c=CreateConfigCommandSetDefrostGroup(7,payload,80);
 CHECK(c.dataLength==81);CHECK(BuildCommandBuffer(c,buffer,sizeof(buffer))==86);
 CHECK(BuildCommandBuffer(c,buffer,85)==0);
 // Exercise the actual server response parser at PID/log/diagnostics/max lengths.
 const unsigned sizes[]={0,60,74,82,85,89,105,113};
 for(unsigned k=0;k<sizeof(sizes)/sizeof(sizes[0]);++k){
  unsigned n=sizes[k];memset(buffer,0,sizeof(buffer));buffer[0]=3;buffer[1]=9;buffer[3]=n;
  for(unsigned i=0;i<n;++i)buffer[4+i]=i;
  uint16_t crc=CalculateCommandCRC(buffer,n+4);memcpy(buffer+n+4,&crc,2);
  CommandResponse response;
  CHECK(ParseResponseBuffer(buffer,n+6,response));CHECK(response.dataLength==n);
  CHECK(memcmp(response.data,buffer+4,n)==0);
  CHECK(WriteToServerWithSync(buffer,n+6)==0);CHECK(sent==n+8);
  CHECK(wire[0]==0xaa&&wire[1]==0x55&&memcmp(wire+2,buffer,n+6)==0);
  CHECK(!ParseResponseBuffer(buffer,n+5,response));
  buffer[n+4]^=1;CHECK(!ParseResponseBuffer(buffer,n+6,response));
 }
 CHECK(WriteToServerWithSync(buffer,126)==0&&sent==128);
 CHECK(WriteToServerWithSync(buffer,127)==MB_ERROR_WRONG_VALUE);
 CHECK(WriteToServerWithSync(buffer,0)==MB_ERROR_WRONG_VALUE);
 CHECK(WriteToServerWithSync(nullptr,1)==MB_ERROR_WRONG_VALUE);
 c.dataLength=124;CHECK(BuildCommandBuffer(c,buffer,sizeof(buffer))==0);
 return 0;
}
