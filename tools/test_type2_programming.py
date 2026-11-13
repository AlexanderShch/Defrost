"""PT21A01 programming regression test with a device applying settings on power cycle.
Run in a Visual Studio developer shell or with g++/clang++ on PATH.
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
#include <vector>
using MB_Error_t=int;using MB_Reg_t=int;using MB_Command_t=int;using MultWR_t=int8_t[8];
constexpr int MB_ERROR_NO=0,MB_ERROR_WRONG_VALUE=3,MB_ERROR_UART_RECIEVE=6;
constexpr int MB_CMD_READ_REGS=3,MB_CMD_WRITE_REG=6,MB_CMD_WRITE_REGS=16;
constexpr int Type2_T=0,Type2_Addr=0xfd,Type2_Baud=0xfe,BAUD_RATE_NUMBER=8;
int BaudRate_Type2[]={1200,2400,4800,9600,19200,38400,57600,115200};
uint8_t SensPortNumber=1,SensBaudRateIndex=3,SensNullValue=255,s_progWriteStage=0;
struct Model{static int address;static int getCurrentAddress_PR(){return address;}};int Model::address=1;
struct MB_Active_t{uint8_t Tx_Buffer[101]{},Rx_Buffer[101]{};};
struct Call{int address,command,reg,value,baud;};std::vector<Call> calls;
int uartBaud=9600,delayedMs=0,activeAddress=1,activeBaud=3,savedAddress=1,savedBaud=3;
int failAt=-1,failure=6,corruptAt=-1;bool corruptRead=false;
void PR_UART4_Init(int baud){uartBaud=baud;}
void osDelay(int ms){delayedMs+=ms;}
MB_Error_t Master_RW(MB_Active_t* pr,int address,MB_Command_t command,MB_Reg_t reg,uint16_t value,MultWR_t){
 calls.push_back({address,command,reg,value,uartBaud});
 int step=(int)calls.size();
 memset(pr->Tx_Buffer,0,101);memset(pr->Rx_Buffer,0,101);
 pr->Tx_Buffer[0]=(uint8_t)address;pr->Tx_Buffer[1]=(uint8_t)command;
 pr->Tx_Buffer[2]=(uint8_t)(reg>>8);pr->Tx_Buffer[3]=(uint8_t)reg;
 pr->Tx_Buffer[4]=(uint8_t)(value>>8);pr->Tx_Buffer[5]=(uint8_t)value;
 if(step==failAt)return failure;
 if(address!=activeAddress||uartBaud!=BaudRate_Type2[activeBaud])return 6;
 if(command==3){assert(reg==Type2_T&&value==1);pr->Rx_Buffer[0]=(uint8_t)address;pr->Rx_Buffer[2]=2;
  if(corruptRead)pr->Rx_Buffer[0]^=1;return 0;}
 // A sensor may write FC10 data but send a response rejected by the old code.
 if(command==16){savedAddress=104;savedBaud=4;return 6;}
 assert(command==6);
 if(reg==Type2_Baud)savedBaud=value;
 else{assert(reg==Type2_Addr);savedAddress=value;}
 memcpy(pr->Rx_Buffer,pr->Tx_Buffer,8);
 if(step==corruptAt)pr->Rx_Buffer[5]^=1;
 return 0;
}
void reset(){calls.clear();activeAddress=savedAddress=SensPortNumber=Model::address=1;
 activeBaud=savedBaud=SensBaudRateIndex=3;uartBaud=9600;delayedMs=0;
 failAt=corruptAt=-1;corruptRead=false;s_progWriteStage=0;}
"""
SUFFIX = r"""
int main(){
 MB_Active_t pr;MultWR_t unused{};
 reset();assert(Master_RW(&pr,1,16,Type2_Addr,2,unused)==6);
 assert(savedAddress==104&&savedBaud==4&&activeAddress==1&&activeBaud==3);
 std::puts("REPRODUCED: FC10 writes settings but its response reports Error 6");
 for(int oldBaud=0;oldBaud<8;++oldBaud)for(int newBaud=0;newBaud<8;++newBaud){
  reset();activeBaud=savedBaud=SensBaudRateIndex=oldBaud;
  assert(Type2_Program(&pr,104,newBaud)==0);
  assert(calls.size()==3&&calls[0].command==3&&calls[1].command==6&&calls[2].command==6);
  assert(calls[1].reg==Type2_Baud&&calls[1].value==newBaud);
  assert(calls[2].reg==Type2_Addr&&calls[2].value==104);
  for(const auto& c:calls)assert(c.address==1&&c.baud==BaudRate_Type2[oldBaud]);
  assert(savedAddress==104&&savedBaud==newBaud&&activeAddress==1&&activeBaud==oldBaud);
  assert(SensPortNumber==1&&SensBaudRateIndex==oldBaud&&Model::address==1&&delayedMs==100);
  // Simulate power cycling: only now do the pending settings become active.
  activeAddress=savedAddress;activeBaud=savedBaud;
  SensPortNumber=Model::address=activeAddress;SensBaudRateIndex=activeBaud;
  assert(Type2_Program(&pr,105,4)==0&&savedAddress==105);
 }
 for(int step=1;step<=3;++step)for(int error=4;error<=7;++error){
  reset();failAt=step;failure=error;
  assert(Type2_Program(&pr,104,4)==error&&s_progWriteStage==step&&(int)calls.size()==step);
 }
 for(int step=2;step<=3;++step){reset();corruptAt=step;
  assert(Type2_Program(&pr,104,4)==6&&s_progWriteStage==step&&(int)calls.size()==step);}
 reset();corruptRead=true;assert(Type2_Program(&pr,104,4)==6&&s_progWriteStage==1&&calls.size()==1);
 reset();assert(Type2_Program(&pr,104,8)==3&&calls.empty());
 reset();assert(Type2_Program(&pr,0,4)==3&&calls.empty());
 reset();assert(Type2_Program(&pr,248,4)==3&&calls.empty());
 reset();Model::address=255;assert(Type2_Program(&pr,104,4)==3&&calls.empty());
 std::puts("PASS: all 64 baud transitions, deferred application, programming after power cycle, transport failures, echo mismatch, stage codes, invalid inputs");
}
"""

def main():
    source = (ROOT / "STM32CubeIDE/ProjectCode/ModBus.cpp").read_text(encoding="utf-8")
    program = source.split("// TYPE2_PROGRAM_BEGIN\n")[1].split("// TYPE2_PROGRAM_END")[0]
    dispatch = source.split("switch (programmedType)")[1].split("case 5:")[0].split("case 2:")[1]
    assert "Type2_Program(PR, WR_Address, WR_BaudRate)" in dispatch
    assert "MB_CMD_WRITE_REGS" not in dispatch
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("Run in a Visual Studio developer shell or put g++/clang++ on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_eeprom_") as folder:
        folder = Path(folder)
        cpp = folder / "type2_program_test.cpp"
        exe = folder / "type2_program_test.exe"
        cpp.write_text(PREFIX + program + SUFFIX, encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            args = [compiler, "/nologo", "/EHsc", "/std:c++14", "/utf-8", str(cpp),
                    "/Fo" + str(folder / "type2_program_test.obj"), "/Fe" + str(exe)]
        else:
            args = [compiler, "-std=c++14", str(cpp), "-o", str(exe)]
        subprocess.run(args, cwd=folder, check=True)
        subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    main()
