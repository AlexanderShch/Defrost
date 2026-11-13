"""EEPROM regression tests using the actual driver with a timed M24C16 model.
Run in a Visual Studio developer shell or with g++/clang++ on PATH.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
PREFIX = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
using HAL_StatusTypeDef=int;
const int HAL_OK=0,HAL_ERROR=1,HAL_BUSY=2,HAL_TIMEOUT=3,I2C_MEMADD_SIZE_8BIT=1;
struct I2C_HandleTypeDef{};I2C_HandleTypeDef hi2c3;
using osMutexId_t=void*;
enum osKernelState_t{osKernelInactive,osKernelRunning,osKernelLocked};
osKernelState_t kernel=osKernelRunning;
const uint32_t osMutexRecursive=1,osMutexPrioInherit=2,osWaitForever=0xffffffff;
const int osOK=0;
struct osMutexAttr_t{const char*name;uint32_t attr_bits;void*cb_mem;uint32_t cb_size;};
thread_local int mutexDepth=0;bool allocationFail=false;
std::recursive_mutex busMutex;std::atomic<bool> touchOwnsI2c{false},waiting{false};
osKernelState_t osKernelGetState(){return kernel;}
int osKernelLock(){return 0;}
void osKernelRestoreLock(int){}
osMutexId_t osMutexNew(const osMutexAttr_t* a){assert(a->attr_bits&osMutexRecursive);return allocationFail?nullptr:(void*)1;}
int osMutexAcquire(osMutexId_t,uint32_t){if(touchOwnsI2c)waiting=true;busMutex.lock();++mutexDepth;return 0;}
void osMutexRelease(osMutexId_t){assert(mutexDepth>0);--mutexDepth;busMutex.unlock();}
namespace EEPROM{constexpr uint16_t kDefaultDeviceAddress=0xa0,kSizeBytes=2048,kPageSizeBytes=16;}
uint8_t memory[2048];uint32_t nowUs=0,busyUntil=0,writeCycleUs=0;
struct Op{bool write;uint16_t dev,addr,size;};std::vector<Op> ops;
int HAL_I2C_IsDeviceReady(I2C_HandleTypeDef*,uint16_t,uint32_t trials,uint32_t){
 if(touchOwnsI2c)return HAL_BUSY;
 for(uint32_t i=0;i<trials;++i){nowUs+=100;if(nowUs>=busyUntil)return HAL_OK;}
 return HAL_ERROR;
}
int HAL_I2C_Mem_Write(I2C_HandleTypeDef*,uint16_t dev,uint16_t addr,int mode,uint8_t* data,uint16_t n,uint32_t){
 assert(mode==I2C_MEMADD_SIZE_8BIT);if(touchOwnsI2c)return HAL_BUSY;
 if(nowUs<busyUntil)return HAL_ERROR;
 assert(n<=16 && (addr%16)+n<=16);
 uint16_t full=((dev&14)>>1)*256+addr;assert(full+n<=2048);
 memcpy(memory+full,data,n);ops.push_back({true,dev,addr,n});busyUntil=nowUs+writeCycleUs;return HAL_OK;
}
int HAL_I2C_Mem_Read(I2C_HandleTypeDef*,uint16_t dev,uint16_t addr,int mode,uint8_t* data,uint16_t n,uint32_t){
 assert(mode==I2C_MEMADD_SIZE_8BIT);if(touchOwnsI2c)return HAL_BUSY;
 if(nowUs<busyUntil)return HAL_ERROR;
 uint16_t full=((dev&14)>>1)*256+addr;assert(full+n<=2048 && addr+n<=256);
 memcpy(data,memory+full,n);ops.push_back({false,dev,addr,n});return HAL_OK;
}


uint32_t HAL_GetTick(){return nowUs/1000;}
void HAL_Delay(uint32_t n){nowUs+=n*1000;}
uint32_t osKernelGetTickFreq(){return 1000;}
void osDelay(uint32_t ticks){nowUs+=ticks*1000;}
"""
SUFFIX = r"""
int main(){
 uint8_t input[64],output[64];for(int i=0;i<64;++i)input[i]=(uint8_t)i;
 assert(EEPROM::Init(0xa0,100));
 writeCycleUs=5000;
 assert(EEPROM::Write(0x100,input,64,100)==HAL_OK);assert(ops.size()==4);
 for(int i=0;i<4;++i)assert(ops[i].dev==0xa2&&ops[i].addr==i*16&&ops[i].size==16);
 assert(nowUs>=20000&&mutexDepth==0);
 assert(EEPROM::Read(0x100,output,64,100)==HAL_OK&&memcmp(input,output,64)==0);
 assert(EEPROM::Write(0x140,input,64,100)==HAL_OK);
 assert(EEPROM::Read(0x140,output,64,100)==HAL_OK&&memcmp(input,output,64)==0);
 assert(EEPROM::Write(2040,input,16,100)==HAL_ERROR);
 assert(EEPROM::Write(0x1f8,input,32,100)==HAL_OK);
 assert(EEPROM::Read(0x1f8,output,32,100)==HAL_OK&&memcmp(input,output,32)==0);
 std::puts("PASS: 5ms page writes, both journal slots, page/block boundaries, range checks");
 busyUntil=nowUs+200000;uint32_t started=HAL_GetTick();
 assert(!EEPROM::Init(0xa0,10));assert(HAL_GetTick()-started>=10&&HAL_GetTick()-started<20);
 busyUntil=nowUs;
 kernel=osKernelInactive;
 assert(EEPROM::Write(0x100,input,4,100)==HAL_OK);
 kernel=osKernelRunning;
 assert(I2C3_BusLock());touchOwnsI2c=true;waiting=false;
 int result=-1;
 std::thread writer([&]{result=EEPROM::Write(0x100,input,4,100);assert(mutexDepth==0);});
 while(!waiting)std::this_thread::yield();
 touchOwnsI2c=false;I2C3_BusUnlock();writer.join();
 assert(result==HAL_OK&&mutexDepth==0);
 assert(EEPROM::Lock());assert(EEPROM::Read(0x100,output,4,100)==HAL_OK);EEPROM::Unlock();
 EEPROM::s_mutex=nullptr;allocationFail=true;
 assert(EEPROM::Write(0x100,input,4,100)==HAL_BUSY);
 std::puts("PASS: ready timeout, pre-RTOS delay, concurrent touch/EEPROM, recursive lock, allocation failure");
}
"""

def main():
    driver = (ROOT / "STM32CubeIDE/ProjectCode/EEPROM.cpp").read_text(encoding="utf-8")
    driver = "\n".join(line for line in driver.splitlines() if not line.startswith("#include"))
    touch = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
    for name in ("I2C3_WriteData", "I2C3_ReadData", "I2C3_ReadBuffer"):
        start = touch.index("static ", touch.index(name + "(") - 16)
        start = touch.index(name + "(", start)
        # Definitions have a newline before their body, declarations do not.
        start = touch.index(name + "(", start + 1) if ";" in touch[start:touch.index("\n", start)] else start
        body = touch[start:touch.index("\n}", start)]
        assert body.index("I2C3_BusLock()") < body.index("HAL_I2C_Mem_") < body.index("I2C3_BusUnlock()")
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("Run in a Visual Studio developer shell or put g++/clang++ on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_eeprom_") as folder:
        folder = Path(folder)
        cpp = folder / "eeprom_test.cpp"
        exe = folder / "eeprom_test.exe"
        cpp.write_text(PREFIX + driver + SUFFIX, encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            args = [compiler, "/nologo", "/EHsc", "/std:c++14", "/utf-8", str(cpp),
                    "/Fo" + str(folder / "eeprom_test.obj"), "/Fe" + str(exe)]
        else:
            args = [compiler, "-std=c++14", str(cpp), "-o", str(exe)]
        subprocess.run(args, cwd=folder, check=True)
        subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    main()
