"""Compile the production TX pipeline with an emulated RTOS/UART and run regression checks.
Run with Python from a Visual Studio developer shell (cl on PATH), or with g++/clang++.
The timings are a model at 19200 8N1, not hardware measurements.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PREFIX = r"""
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <deque>
#include <vector>
struct ServerTxItem_t{uint8_t PacketLength;uint8_t data[96];uint8_t Reserved;};
static_assert(sizeof(ServerTxItem_t)==98,"queue size");
struct Queue{std::deque<ServerTxItem_t> items;size_t cap;};
using osMessageQueueId_t=Queue*;
Queue normal{{},9},telemetry{{},4};
osMessageQueueId_t Data_QueueHandle=&normal,Telemetry_QueueHandle=&telemetry;
int ServerResponseReceived_SemHandle=1;
const int osOK=0,MB_ERROR_NO=0,CMD_TYPE_REQUEST=3,REQ_CMD_SEND_STATE=8;
uint32_t ticks=0;bool ack=false,handling=false,autoAck=false,injectNack=false;
int failures=0,calls=0,idle=0;
std::vector<ServerTxItem_t> sent;
struct Stop{};
bool ServerTx_EnqueueHighPriority(const uint8_t*,uint16_t);
void ResendLastTelemetry();
uint32_t osKernelGetTickFreq(){return 1000;}
void osDelay(uint32_t t){ticks+=t;}
int osMessageQueuePut(Queue* q,const void* p,uint8_t,uint32_t wait){
 assert(wait==0);if(q->items.size()==q->cap)return 1;q->items.push_back(*(const ServerTxItem_t*)p);return 0;
}
int osMessageQueueGet(Queue* q,void* p,void*,uint32_t wait){
 if(q->items.empty()){ticks+=wait;if(++idle>6 && normal.items.empty() && telemetry.items.empty())throw Stop{};return 1;}
 idle=0;*(ServerTxItem_t*)p=q->items.front();q->items.pop_front();return 0;
}
int osSemaphoreAcquire(int,uint32_t wait){
 if(ack){ack=false;return osOK;}ticks+=wait;return 1;
}
ServerTxItem_t frame(int code,int id,int len=66){ServerTxItem_t x{};x.PacketLength=(uint8_t)len;x.data[0]=3;x.data[1]=(uint8_t)code;x.data[3]=(uint8_t)(len-6);x.data[4]=(uint8_t)id;return x;}
int WriteToServerWithSync(uint8_t* p,int n){
 assert(!handling);++calls;if(failures>0){--failures;return 6;}
 ServerTxItem_t x{};x.PacketLength=(uint8_t)n;memcpy(x.data,p,n);sent.push_back(x);
 ticks+=((n+2)*10*1000+19199)/19200+6; // wire time + UART arbitration/direction margin
 if(p[0]==3&&p[1]==8){
  if(injectNack){
   injectNack=false;auto b=frame(8,0xb2);assert(ServerTx_EnqueueHighPriority(b.data,b.PacketLength));
   auto d=frame(10,0xd3,91);assert(ServerTx_EnqueueHighPriority(d.data,d.PacketLength));
   handling=true;ResendLastTelemetry();handling=false;ack=true;
  }else if(autoAck)ack=true;
 }
 return 0;
}
"""
SUFFIX = r"""

void init(){
 normal.items.clear();telemetry.items.clear();sent.clear();ticks=0;calls=failures=idle=0;
 ack=handling=autoAck=injectNack=false;s_lastSentTelemetryPacketLen=0;s_telemetryRetryPending=0;g_serverTxRejected=0;
}
void run(){try{TX_ToServer();}catch(Stop&){} }
int main(){
 auto a=frame(8,0xa1),diag=frame(10,0xd3,91),log=frame(1,0xc4,95);log.data[0]=0;
 init();for(int i=0;i<9;++i)assert(ServerTx_EnqueueHighPriority(diag.data,91));
 assert(!ServerTx_EnqueueHighPriority(diag.data,91));assert(g_serverTxRejected==1);
 assert(ServerTx_EnqueueHighPriority(a.data,66));run();
 assert(sent.size()==10&&sent.front().data[4]==0xa1&&s_lastSentTelemetryPacket[4]==0xa1);
 init();injectNack=true;assert(ServerTx_EnqueueHighPriority(a.data,66));run();
 assert(sent.size()==4&&sent[0].data[4]==0xa1&&sent[1].data[4]==0xa1&&sent[2].data[4]==0xb2&&sent[3].data[1]==10);
 init();Data_SaveLastSentTelemetryPacket(a.data,66);
 for(int i=0;i<9;++i)assert(ServerTx_EnqueueHighPriority(diag.data,91));
 handling=true;for(int i=0;i<20;++i)ResendLastTelemetry();handling=false;assert(calls==0);run();
 assert(sent.size()==10&&sent[0].data[4]==0xa1&&g_serverTxRejected==0);
 init();failures=2;assert(ServerTx_EnqueueHighPriority(a.data,66));run();assert(calls==3&&sent.size()==1);
 init();assert(!ServerTx_EnqueueHighPriority(nullptr,0));assert(!ServerTx_EnqueueHighPriority(a.data,97));
 init();assert(ServerTx_EnqueueHighPriority(a.data,66));assert(ServerTx_EnqueueHighPriority(diag.data,91));assert(ServerTx_EnqueueHighPriority(log.data,95));run();
 assert(sent.size()==3&&ticks<1000);std::printf("PASS: telemetry+diag+log, simulated %u ms including 100 ms ACK timeout\n",ticks);
 init();Data_SaveLastSentTelemetryPacket(a.data,66);ResendLastTelemetry();
 assert(ServerTx_EnqueueHighPriority(a.data,66));assert(ServerTx_EnqueueHighPriority(diag.data,91));assert(ServerTx_EnqueueHighPriority(log.data,95));run();
 assert(sent.size()==4&&ticks<1000);std::printf("PASS: with one retry, simulated %u ms including two 100 ms ACK timeouts\n",ticks);
 std::puts("PASS: queue saturation, retry priority, exact retransmission despite newer queued snapshot, UART failure retention, RX nonblocking, packet bounds");
}
"""

def main():
    source = (ROOT / "STM32CubeIDE/ProjectCode/Data.cpp").read_text(encoding="utf-8")
    body = source[source.index("#define TELEMETRY_PACKET_MAX"):]
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("Run in a Visual Studio developer shell or put g++/clang++ on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_server_tx_") as folder:
        folder = Path(folder)
        source_file = folder / "server_tx_test.cpp"
        executable = folder / "server_tx_test.exe"
        source_file.write_text(PREFIX + body + SUFFIX, encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            args = [compiler, "/nologo", "/EHsc", "/std:c++14", "/utf-8", str(source_file),
                    "/Fo" + str(folder / "server_tx_test.obj"), "/Fe" + str(executable)]
        else:
            args = [compiler, "-std=c++14", str(source_file), "-o", str(executable)]
        subprocess.run(args, cwd=folder, check=True)
        subprocess.run([str(executable)], check=True)

if __name__ == "__main__":
    main()
