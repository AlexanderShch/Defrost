"""Regression tests for programming screen transitions using the actual view code.
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
#include <cstdarg>
#include <initializer_list>
constexpr int SQ=7,STQ=6,TYPE5_BAUD_RATE_NUMBER=7,BAUD_RATE_NUMBER=8;
using MB_Error_t=int; const int MB_ERROR_NO=0,MB_ERROR_EEPROM=8;
struct SensorEepromDiagnostic{uint8_t stage,halStatus;uint32_t i2cError;};
SensorEepromDiagnostic ProgrammingSensor_GetEepromDiagnostic(){return {4,1,4};}
uint8_t ProgrammingSensor_GetWriteStage(){return 2;}
bool warning=false;uint8_t ProgrammingSensor_Needs19200(){return warning;}
uint8_t ProgrammingSensor_TakeWriteResult(MB_Error_t*){return 0;}
void ProgrammingSensor_Start(){}void ProgrammingSensor_RequestStop(){}
struct SENSOR_Type_t{int TypeNumber;const char*TypeName;};
struct SENSOR_typedef_t{uint8_t Address,BaudRate,TypeOfSensor;const char*PositionName;};
SENSOR_Type_t Sensor_type[STQ]={{0,"Null"},{1,"TH"},{2,"T"},{3,"BT"},{4,"IO"},{5,"DS18B20"}};
SENSOR_typedef_t Sensor_array[SQ]={{101,3,1,"Left def"},{102,3,1,"Right def"},{103,3,1,"Center def"},{104,4,2,"Left prod"},{105,4,2,"Right prod"},{106,4,2,"Body def"},{2,3,4,"IO"}};
uint8_t SensNullValue=255;
int BaudRate_Type1[]={2400,4800,9600,19200,38400,57600,115200,1200};
int BaudRate_Type2[]={1200,2400,4800,9600,19200,38400,57600,115200};
int BaudRate_Type5[]={1200,2400,4800,9600,19200,38400,57600};
int BaudRate_Type4[]={2400,4800,9600,19200,38400,57600,115200,1200};
bool SensorConfig_IsTypeAllowed(uint8_t,uint8_t);
uint8_t SensorConfig_DestinationCount(uint8_t),SensorConfig_DestinationChannel(uint8_t,uint8_t),SensorConfig_BaudCount(uint8_t);
struct Model{static int Flag_Alert,Type_of_sensor;static void setCurrentVal_PR(int,int){}};
int Model::Flag_Alert=0,Model::Type_of_sensor=0;
struct Presenter{int writes=0;uint8_t speed=0,address=0;
 void PR_Sensor_Type(int t){Model::Type_of_sensor=t;}
 void PR_Sensor_Data_Write(uint8_t s,uint8_t a){++writes;speed=s;address=a;}
};
namespace touchgfx {
namespace Unicode {
 using UnicodeChar=char;
 void snprintf(char* b,int n,const char* fmt,...){va_list a;va_start(a,fmt);vsnprintf(b,n,fmt,a);va_end(a);}
 void strncpy(char* b,const char* s,int n){std::strncpy(b,s,n);b[n-1]=0;}
 void fromUTF8(const uint8_t* s,char* b,int n){strncpy(b,(const char*)s,n);}
}
constexpr int WIDE_TEXT_WORDWRAP=0;
}
namespace Unicode=touchgfx::Unicode;
struct Widget{bool visible=false,touchable=true;int count=0,selected=0;
 void setVisible(bool b){visible=b;}bool isVisible()const{return visible;}
 void setTouchable(bool b){touchable=b;}void invalidate(){}
 void setNumberOfItems(int n){count=n;}void animateToItem(int n,int){selected=n;}
 template<class T>void setItemSelectedCallback(T&){}
 int getTypedText(){return 0;}int getColor(){return 0;}
 void setTypedText(int){}void setWildcard(char*){}void setPosition(int,int,int,int){}
 void setWideTextAction(int){}void setColor(int){}
};
namespace touchgfx{using TextAreaWithOneWildcard=Widget;}
struct TextContainer{void updateTextItem(char*,int){}};
struct TextSelectedContainer{void updateTextSelectedItem(char*,int){}};
struct ScrollItemContainer{void updateScrollItem(int){}};
struct ScrollSelectedItemContainer{void updateScrollSelectedItem(int){}};
template<class T,class A>struct Callback{Callback(T*,void(T::*)(A)){}};
struct Settings5ViewBase{
 Presenter actualPresenter;Presenter*presenter=&actualPresenter;
 Widget BTNWrite,BTNConfirm,BTNCancel,BTNSensorType,BTNSetSpeed,BTNSetAddress;
 Widget scrollSensorType,scrollSensorSpeedNew,scrollSensorAddressNew,Alert_Background,Alert_Message,SensorCurrentSpeed,SensorCurrentAddress;
 enum{BTNSETSPEED_SIZE=64,BTNSETADDRESS_SIZE=64,BTNSENSORTYPE_SIZE=64,SENSORCURRENTSPEED_SIZE=64,SENSORCURRENTADDRESS_SIZE=64};
 char BTNSetSpeedBuffer[64]{},BTNSetAddressBuffer[64]{},BTNSensorTypeBuffer[64]{},SensorCurrentSpeedBuffer[64]{},SensorCurrentAddressBuffer[64]{};
 void setupScreen(){}void tearDownScreen(){}void add(Widget&){}void invalidate(){}
};
"""
SUFFIX = r"""
struct TestView:Settings5View{
 using Settings5View::scrollSensorTypeItemSelectedHandler;
 using Settings5View::scrollSensorSpeedNewItemSelectedHandler;
 using Settings5View::scrollSensorAddressNewItemSelectedHandler;
 using Settings5View::writeErrorMessage;
 void buttons(bool write,bool confirm,bool cancel){
  assert(BTNWrite.visible==write&&BTNWrite.touchable==write);
  assert(BTNConfirm.visible==confirm&&BTNConfirm.touchable==confirm);
  assert(BTNCancel.visible==cancel&&BTNCancel.touchable==cancel);
 }
 void readings(){Val_Addr_UpdateView(104);Val_BaudRate_UpdateView(4);}
 void ready(int type){setupScreen();BTNSensorTypeClicked();scrollSensorTypeItemSelectedHandler(type);BTNConfirmClicked();
  BTNSetSpeedClicked();scrollSensorSpeedNewItemSelectedHandler(4);BTNConfirmClicked();
  BTNSetAddressClicked();BTNConfirmClicked();readings();buttons(true,false,false);}
};
int main(){
 for(int type: {1,2,4,5}){
  TestView v;v.ready(type);
  // Polling callbacks cannot replace Accept/Cancel with Write for any wheel.
  v.BTNSensorTypeClicked();v.readings();v.buttons(false,true,true);
  v.BTNWriteClicked();assert(v.actualPresenter.writes==0);v.BTNCancelClicked();
  v.BTNSetSpeedClicked();v.scrollSensorSpeedNewItemSelectedHandler(3);v.readings();v.buttons(false,true,true);
  v.BTNWriteClicked();assert(v.actualPresenter.writes==0);v.BTNConfirmClicked();v.buttons(true,false,false);
  v.BTNSetAddressClicked();v.readings();v.buttons(false,true,true);v.BTNCancelClicked();v.buttons(true,false,false);
  // One request; async readings and repeated touches cannot start another.
  v.BTNWriteClicked();v.buttons(false,false,false);v.readings();v.BTNWriteClicked();
  v.BTNSetSpeedClicked();v.BTNConfirmClicked();v.BTNCancelClicked();v.buttons(false,false,false);
  assert(v.actualPresenter.writes==1);
  for(bool warn: {false,true}){
   warning=warn;v.WriteSaved_Output();v.readings();v.buttons(false,true,false);
   const bool needsPowerCycle=type==2||type==4;
   assert(v.Alert_Background.visible);
   assert(v.writeErrorMessage.visible==(!needsPowerCycle||warn));
   assert(v.Alert_Message.visible==(needsPowerCycle&&!warn));
   v.BTNSetSpeedClicked();v.BTNWriteClicked();assert(v.actualPresenter.writes==1);
   v.BTNConfirmClicked();
   if(needsPowerCycle&&warn){
    v.buttons(false,true,false);v.readings();v.buttons(false,true,false);
    assert(v.Alert_Message.visible&&!v.writeErrorMessage.visible);
    v.BTNWriteClicked();assert(v.actualPresenter.writes==1);v.BTNConfirmClicked();
   }
   v.buttons(true,false,false);
   assert(!v.Alert_Background.visible&&!v.writeErrorMessage.visible&&!v.Alert_Message.visible);
   v.BTNSetSpeedClicked();v.scrollSensorSpeedNewItemSelectedHandler(4);v.readings();v.buttons(false,true,true);
   v.BTNConfirmClicked();assert(v.actualPresenter.writes==1);v.buttons(true,false,false);
  }
  v.WriteError_Output(8);v.readings();v.buttons(false,true,false);v.BTNConfirmClicked();
  assert(!v.writeErrorMessage.visible);v.buttons(true,false,false);
  Model::Flag_Alert=1;v.Alert_Message_Output();v.readings();v.buttons(false,true,false);
  v.BTNConfirmClicked();assert(Model::Flag_Alert==0&&!v.Alert_Message.visible);v.buttons(true,false,false);
  // Both current values must be valid, regardless of update order.
  v.Val_Addr_UpdateView(255);v.Val_BaudRate_UpdateView(4);v.buttons(false,false,false);
  v.readings();v.Val_BaudRate_UpdateView(255);v.Val_Addr_UpdateView(104);v.buttons(false,false,false);
 }
 TestView v;v.ready(5);
 v.BTNSetSpeedClicked();v.scrollSensorSpeedNewItemSelectedHandler(2);v.BTNCancelClicked();assert(SetSpeed==4);
 v.BTNSetAddressClicked();v.scrollSensorAddressNewItemSelectedHandler(2);v.BTNCancelClicked();assert(SetAddress==104);
 v.BTNSensorTypeClicked();v.scrollSensorTypeItemSelectedHandler(1);v.BTNCancelClicked();assert(SetSensor==5);
 v.BTNSensorTypeClicked();v.scrollSensorTypeItemSelectedHandler(1);v.BTNConfirmClicked();v.buttons(false,false,false);
 assert(SetAddress==0&&SetSpeed==255);
 std::puts("PASS: types 1/2/4/5, polling during wheels and dialogs, repeat-write guard, success/warning/error closure, cancel and type changes");
}
"""

def main():
    header = (ROOT / "TouchGFX/gui/include/gui/settings5_screen/Settings5View.hpp").read_text(encoding="utf-8")
    header = "\n".join(line for line in header.splitlines() if not line.startswith("#include"))
    source = (ROOT / "TouchGFX/gui/src/settings5_screen/Settings5View.cpp").read_text(encoding="utf-8")
    source = "\n".join(line for line in source.splitlines() if not line.startswith("#include"))
    modbus = (ROOT / "STM32CubeIDE/ProjectCode/ModBus.cpp").read_text(encoding="utf-8")
    policy = modbus.split("// SENSOR_ASSIGNMENT_POLICY_BEGIN\n")[1].split("// SENSOR_ASSIGNMENT_POLICY_END")[0]
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("Run in a Visual Studio developer shell or put g++/clang++ on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_eeprom_") as folder:
        folder = Path(folder)
        cpp = folder / "programming_ui_test.cpp"
        exe = folder / "programming_ui_test.exe"
        cpp.write_text(PREFIX + header + source + policy + SUFFIX, encoding="utf-8")
        if Path(compiler).stem.lower() == "cl":
            args = [compiler, "/nologo", "/EHsc", "/std:c++14", "/utf-8", str(cpp),
                    "/Fo" + str(folder / "programming_ui_test.obj"), "/Fe" + str(exe)]
        else:
            args = [compiler, "-std=c++14", str(cpp), "-o", str(exe)]
        subprocess.run(args, cwd=folder, check=True)
        subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    main()
