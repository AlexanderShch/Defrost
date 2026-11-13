"""Sensor journal regression tests using actual assignment/storage code and simulated power cuts.
Run in a Visual Studio developer shell or with g++/clang++ on PATH.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
PREFIX = r"""#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
constexpr int SQ=7;
using MB_Error_t=int;
constexpr int MB_ERROR_NO=0,MB_ERROR_EEPROM=8,MB_ERROR_WRONG_VALUE=3,MB_ERROR_WRONG_ADDRESS=2,HAL_OK=0;
struct SensorEntry {uint8_t Address,BaudRate,TypeOfSensor,Active=0;};
SensorEntry Sensor_array[SQ]={{101,3,1},{102,3,1},{103,3,1},{104,4,2},{105,4,2},{106,4,2},{2,3,4}};
uint16_t MB_GetCRC(volatile uint8_t* p,uint16_t n){
 uint16_t crc=0xffff;while(n--){crc^=*p++;for(int i=0;i<8;++i)crc=(crc&1)?(crc>>1)^0xa001:crc>>1;}return crc;
}
namespace EEPROM {
 constexpr int kSensorConfigSlotBytes=64,kSensorConfigAddress=256,kSensorConfigEnd=384,kSizeBytes=2048;
 uint8_t memory[kSizeBytes];int budget=-1,written=0;bool failReads=false,failLock=false,ignoreWrites=false;
 bool Lock(){return !failLock;}void Unlock(){}
 int Read(uint16_t a,uint8_t* p,uint16_t n){assert(a+n<=kSizeBytes);if(failReads)return 1;memcpy(p,memory+a,n);return 0;}
 int Write(uint16_t a,const uint8_t* p,uint16_t n){
  assert(a>=256&&a+n<=384);if(ignoreWrites)return 0;for(int i=0;i<n;++i){if(budget==0)return 1;memory[a+i]=p[i];++written;if(budget>0)--budget;}return 0;
 }
}
constexpr int TYPE5_BAUD_RATE_NUMBER=7, BAUD_RATE_NUMBER=8;

const int HAL_ERROR=1,HAL_BUSY=2;
using HAL_StatusTypeDef=int;
struct I2C_HandleTypeDef{}; I2C_HandleTypeDef hi2c3;
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef*){return 4;}
struct SensorEepromDiagnostic{uint8_t stage,halStatus;uint32_t i2cError;};
"""
SUFFIX = r"""void crcFix(uint8_t* p){uint16_t c=MB_GetCRC(p,62);p[62]=(uint8_t)c;p[63]=(uint8_t)(c>>8);}
int main(){
 using namespace EEPROM;
 const int counts[6]={0,3,3,0,1,3};
 for(uint8_t type=0;type<6;++type){
  assert(SensorConfig_DestinationCount(type)==counts[type]);
  for(uint8_t item=0;item<counts[type];++item){
   uint8_t channel=SensorConfig_DestinationChannel(type,item);
   assert(SensorConfig_IsTypeAllowed(channel,type));
   assert(channel==(type==1?item:type==4?6:item+3));
  }
  assert(SensorConfig_DestinationChannel(type,counts[type])==SQ);
  for(uint8_t baud=0;baud<8;++baud){
   uint8_t channel=SensorConfig_DestinationChannel(type,0);
   bool supported=counts[type]>0 && baud<(type==5?7:8);
   assert(ConfigEntryValid(channel,104,baud,type)==supported);
   assert(SensorConfig_IsWorkingBaud(type,baud)==(supported&&baud==((type==2||type==5)?4:3)));
  }
 }

 for(uint8_t channel=3;channel<=5;++channel){
  for(uint8_t type=0;type<=5;++type){
   const uint8_t speed=(type==2||type==5)?4:3;
   assert(ConfigEntryValid(channel,104+channel-3,speed,type)==(type==2||type==5));
  }
 }

 std::memset(memory,0xff,sizeof(memory));SensorConfig_Load(true);
 assert(Sensor_array[3].TypeOfSensor==2 && s_sensorConfigSlot==-1);
 uint8_t destination=255;
 assert(ConfigCheckProgramming(104,4,5,&destination)==0 && destination==3);
 assert(ConfigCheckProgramming(104,3,5,&destination)==0);
 assert(ConfigCheckProgramming(101,4,5,&destination)==3);
 assert(ConfigCheckProgramming(2,4,5,&destination)==3);
 assert(ConfigSaveProgramming(3,104,4,5)==0);
 assert(Sensor_array[3].TypeOfSensor==2); // live polling unchanged until boot
 SensorConfig_Load(true);assert(Sensor_array[3].TypeOfSensor==5);
 uint8_t snapshot[2048];memcpy(snapshot,memory,sizeof(memory));
 // Every possible interrupted byte of the invalidate/payload/commit transaction.
 for(int cutoff=0;cutoff<68;++cutoff){
  memcpy(memory,snapshot,sizeof(memory));budget=-1;SensorConfig_Load(true);budget=cutoff;
  assert(ConfigSaveProgramming(4,105,4,5)==8);
  const auto diagnostic=ProgrammingSensor_GetEepromDiagnostic();
  assert(diagnostic.stage==(cutoff<4?3:cutoff<64?4:5));
  assert(diagnostic.halStatus==HAL_ERROR&&diagnostic.i2cError==4);
  budget=-1;SensorConfig_Load(true);
  assert(Sensor_array[3].TypeOfSensor==5 && Sensor_array[4].TypeOfSensor==2);
 }
 memcpy(memory,snapshot,sizeof(memory));SensorConfig_Load(true);written=0;
 assert(ConfigSaveProgramming(3,104,4,5)==0 && written==0);
 assert(ConfigSaveProgramming(4,105,4,5)==0);SensorConfig_Load(true);
 assert(Sensor_array[3].TypeOfSensor==5 && Sensor_array[4].TypeOfSensor==5);
 memory[256+64+15]^=1;SensorConfig_Load(true);
 assert(Sensor_array[3].TypeOfSensor==5 && Sensor_array[4].TypeOfSensor==2); // old copy
 memory[256+15]^=1;SensorConfig_Load(true);assert(Sensor_array[3].TypeOfSensor==2); // defaults
 memcpy(memory,snapshot,sizeof(memory));SensorConfig_Load(true);
 assert(ConfigSaveProgramming(3,104,3,5)==0);SensorConfig_Load(true);
 assert(Sensor_array[3].TypeOfSensor==5 && Sensor_array[3].BaudRate==3);
 memcpy(memory,snapshot,sizeof(memory));memory[256+4]=7;crcFix(memory+256);SensorConfig_Load(true);
 assert(Sensor_array[3].TypeOfSensor==2); // unsupported version
 memcpy(memory,snapshot,sizeof(memory));memory[256+10+3]=101;crcFix(memory+256);SensorConfig_Load(true);
 assert(Sensor_array[3].TypeOfSensor==2); // duplicate address, even with valid CRC
 memcpy(memory,snapshot,sizeof(memory));failReads=true;SensorConfig_Load(true);
 assert(Sensor_array[3].TypeOfSensor==2);failReads=false;
 SensorConfig_Load(false);assert(ConfigCheckProgramming(104,4,5,&destination)==8);
 assert(Sensor_array[3].TypeOfSensor==2);
 assert(ProgrammingSensor_GetEepromDiagnostic().stage==1);
 // Distinguish mutex, readback transport, write protection, invalid record errors.
 memcpy(memory,snapshot,sizeof(memory));SensorConfig_Load(true);
 failLock=true;assert(ConfigSaveProgramming(4,105,4,5)==8);
 assert(ProgrammingSensor_GetEepromDiagnostic().stage==2);failLock=false;
 failReads=true;assert(ConfigSaveProgramming(4,105,4,5)==8);
 assert(ProgrammingSensor_GetEepromDiagnostic().stage==6);failReads=false;
 memcpy(memory,snapshot,sizeof(memory));SensorConfig_Load(true);
 ignoreWrites=true;assert(ConfigSaveProgramming(4,105,4,5)==8);
 assert(ProgrammingSensor_GetEepromDiagnostic().stage==7);ignoreWrites=false;
 assert(ConfigSaveProgramming(4,105,4,1)==8);
 assert(ProgrammingSensor_GetEepromDiagnostic().stage==8);
 assert(ConfigSaveProgramming(4,105,4,5)==0);
 assert(ProgrammingSensor_GetEepromDiagnostic().stage==0);
 // Sequence number wrap-around.
 memcpy(memory,snapshot,sizeof(memory));memcpy(memory+320,memory+256,64);
 memset(memory+256+6,255,4);crcFix(memory+256);
 memset(memory+320+6,0,4);memory[320+10+4*3+2]=5;crcFix(memory+320);
 SensorConfig_Load(true);assert(Sensor_array[4].TypeOfSensor==5);
 std::puts("PASS: EEPROM journal recovery at 68 power cut points, assignments, speed policy, Error 8 stages 1-8");
}
"""

def main():
    source = (ROOT / "STM32CubeIDE/ProjectCode/ModBus.cpp").read_text(encoding="utf-8")
    policy = source.split("// SENSOR_ASSIGNMENT_POLICY_BEGIN\n")[1].split("// SENSOR_ASSIGNMENT_POLICY_END")[0]
    storage = source.split("// SENSOR_CONFIG_STORAGE_BEGIN\n")[1].split("// SENSOR_CONFIG_STORAGE_END")[0]
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("Run in a Visual Studio developer shell or put g++/clang++ on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_eeprom_") as folder:
        folder = Path(folder)
        cpp = folder / "sensor_config_test.cpp"
        exe = folder / "sensor_config_test.exe"
        cpp.write_text(PREFIX + policy + storage + SUFFIX, encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            args = [compiler, "/nologo", "/EHsc", "/std:c++14", "/utf-8", str(cpp),
                    "/Fo" + str(folder / "sensor_config_test.obj"), "/Fe" + str(exe)]
        else:
            args = [compiler, "-std=c++14", str(cpp), "-o", str(exe)]
        subprocess.run(args, cwd=folder, check=True)
        subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    main()
