"""Regression test for concurrent Modbus RX arming on UART4 and UART5.
Run in a Visual Studio developer shell, or with g++/clang++ on PATH.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PREFIX = r"""
#include <cstdint>
#include <cassert>
#include <cstdio>
struct UART_HandleTypeDef{};
UART_HandleTypeDef huart4,huart5,unknown;
const int HAL_OK=0;
uint8_t* received4=nullptr;uint8_t* received5=nullptr;
uint16_t size4=0,size5=0;bool fail4=false,fail5=false;
int calls4=0,calls5=0,drained=0;UART_HandleTypeDef *prepared=nullptr;
int MB_StartReceiveToIdle(UART_HandleTypeDef* uart,uint8_t* buffer,uint16_t size){
 if(uart==&huart4){++calls4;received4=buffer;size4=size;return fail4?1:0;}
 assert(uart==&huart5);++calls5;received5=buffer;size5=size;return fail5?1:0;
}
const int TX_Compl_SemHandle=1,RX_Compl_SemHandle=2,MB_MASTER_DE_GPIO_Port=0,MB_MASTER_DE_Pin=0;
void DrainBinarySemaphore(int){++drained;}
void MB_PrepareUartForMasterRx(UART_HandleTypeDef* uart,int,int){prepared=uart;}
"""
SUFFIX = r"""int main(){
 uint8_t buf4[101]{},buf5[101]{};
 assert(MB_RxArmFor(&unknown)==nullptr);
 int cases=0;
 for(int prepareOrder=0;prepareOrder<2;++prepareOrder)
 for(int completeOrder=0;completeOrder<2;++completeOrder)
 for(int errors=0;errors<4;++errors){
  s_mbRxArm4={};s_mbRxArm5={};calls4=calls5=0;received4=received5=nullptr;
  fail4=(errors&1)!=0;fail5=(errors&2)!=0;
  if(prepareOrder==0){MB_QueueRxArm(MB_RxArmFor(&huart4),buf4,11);MB_QueueRxArm(MB_RxArmFor(&huart5),buf5,17);}
  else{MB_QueueRxArm(MB_RxArmFor(&huart5),buf5,17);MB_QueueRxArm(MB_RxArmFor(&huart4),buf4,11);}
  if(completeOrder==0){MB_CompleteRxArm(&huart4);assert(s_mbRxArm5.pending==1);MB_CompleteRxArm(&huart5);}
  else{MB_CompleteRxArm(&huart5);assert(s_mbRxArm4.pending==1);MB_CompleteRxArm(&huart4);}
  assert(received4==buf4&&received5==buf5&&size4==11&&size5==17);
  assert(s_mbRxArm4.ok==!fail4&&s_mbRxArm5.ok==!fail5);
  assert(s_mbRxArm4.pending==0&&s_mbRxArm5.pending==0&&calls4==1&&calls5==1);
  MB_CompleteRxArm(&huart4);MB_CompleteRxArm(&huart5);assert(calls4==1&&calls5==1);
  ++cases;
 }
 MB_QueueRxArm(&s_mbRxArm4,buf4,23);MB_QueueRxArm(&s_mbRxArm5,buf5,29);
 MB_Master_RxChannelPrepare();
 assert(s_mbRxArm4.pending==1&&s_mbRxArm4.buffer==buf4&&s_mbRxArm4.size==23);
 assert(s_mbRxArm5.pending==0&&s_mbRxArm5.buffer==nullptr&&drained==2&&prepared==&huart5);
 std::printf("PASS: %d interleavings/error combinations; UART5 reset preserves UART4 programming context\n",cases);
}
"""

def main():
    source = (ROOT / "STM32CubeIDE/ProjectCode/ModBus.cpp").read_text(encoding="utf-8")
    state = source.split("// UART_RX_ARM_STATE_BEGIN\n")[1].split("// UART_RX_ARM_STATE_END")[0]
    callback = source.split("// UART_RX_ARM_CALLBACK_BEGIN\n")[1].split("// UART_RX_ARM_CALLBACK_END")[0]
    start = source.index("void MB_Master_RxChannelPrepare(void)\n{")
    prepare = source[start:source.index("\n}\n", start) + 3]
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("Run in a Visual Studio developer shell or put g++/clang++ on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_rx_arm_") as folder:
        folder = Path(folder)
        cpp = folder / "rx_arm_test.cpp"
        exe = folder / "rx_arm_test.exe"
        cpp.write_text(PREFIX + state + callback + prepare + SUFFIX, encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            args = [compiler, "/nologo", "/EHsc", "/std:c++14", "/utf-8", str(cpp),
                    "/Fo" + str(folder / "rx_arm_test.obj"), "/Fe" + str(exe)]
        else:
            args = [compiler, "-std=c++14", str(cpp), "-o", str(exe)]
        subprocess.run(args, cwd=folder, check=True)
        subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    main()
