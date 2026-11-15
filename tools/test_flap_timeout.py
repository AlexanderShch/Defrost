"""Run actual flap/shutdown functions on a host with simulated I/O (C++14)."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    tail = r"[^;{]*\)\s*\{" if signature.endswith("(") else r"\s*\{"
    match = re.search(re.escape(signature) + tail, source)
    if match is None:
        raise ValueError("Function definition not found: " + signature)
    start = match.start()
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


PREFIX = r'''
#include <cstdio>
#include "DefrostControl.h"
struct Outputs {
 uint8_t Water_Flap=0, _Out=0, _Inj=0;
 uint8_t Vent1_Left=0,Vent2_Left=0,Vent1_Right=0,Vent2_Right=0;
 uint8_t Ten1_Left=0,Ten2_Left=0,Ten1_Right=0,Ten2_Right=0;
};
int returnRhDeci=850;
bool returnRhUsable=true;
namespace Model {
 Outputs DFR, DFR_manual;
 uint8_t Flag_DFR_manual=0;
 uint16_t Device_AlarmFlags=0;
 struct { struct { uint8_t Air_Close=1; } Bits; } DI_DFR;
 int getCurrentVal_H(int) { return returnRhDeci; }
}
DefrostParams_t g_defrostParams = {};
struct State {
 uint8_t flapAlarm=0, flapDesiredOpenPrev=0, flapDesiredOpenPrevValid=0;
 uint16_t flapTransitionElapsed_s=0;
 uint8_t outOn=0, outFanOn=0, outDamperState=0;
 uint16_t outDamperTimer_s=0, outHold_s=0;
 float wDeadband_kgkg=.001f;
 uint16_t shutdownOutFanRemain_s=0, shutdownFlapOpenWait_s=0;
 uint8_t shutdownStage=0, shutdownActive=0, shutdownGateOpening=0;
 uint8_t shutdownGateLiftPulse_s=0, shutdownGateFullOpenActive=0;
 uint8_t shutdownGateOpenReleased=0, shutdownGateOpenSawActive=0, shutdownGateOpenPhase_s=0;
 uint8_t stagedTen1LeftOn=0, stagedTen2LeftOn=0, stagedTen1RightOn=0, stagedTen2RightOn=0;
 uint8_t stagedVent1LeftOn=0, stagedVent2LeftOn=0, stagedVent1RightOn=0, stagedVent2RightOn=0;
 uint8_t shutdownActuatorDelay_s=0, startupActuatorDelay_s=0;
} g;
enum class ShutdownStage : uint8_t { StopActuatorsAndGatePulse=0, Ventilation=1, FullGateOpen=2 };
enum class GateControlCommand { Open };
uint8_t gateActive=0;
void GateControl_SetCommand(GateControlCommand, uint8_t on) { gateActive=on; }
uint8_t GateControl_IsCommandActive(GateControlCommand) { return gateActive; }
enum class LampModeState { AutoActive };
void ApplyModeLamps(LampModeState) {}
constexpr int kSensReturn_T_H=2;
constexpr uint8_t kShutdownGateOpenStuckTimeout_s=15;
constexpr uint16_t kDeviceAlarmVentOutBit=1u<<8;
bool DefrostAirThChannelUsable(int) { return returnRhUsable; }
float DeciToRH(int16_t v) { return v*.1f; }
float Clamp(float v,float lo,float hi) { return v<lo?lo:v>hi?hi:v; }
'''

SUFFIX = r'''
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL line %d: %s\n",__LINE__,#expr); return 1; } } while(0)
void reset(uint16_t timeout, bool checks=true) {
 g=State{}; Model::DFR=Outputs{}; Model::DFR_manual=Outputs{};
 Model::DI_DFR.Bits.Air_Close=1; Model::Device_AlarmFlags=0; Model::Flag_DFR_manual=0;
 g_defrostParams=DefrostParams_t{}; g_defrostParams.outDamperTimer_s=timeout;
 g_defrostParams.outFanDelay_s=5; g_defrostParams.purgeTimeout_s=300;
 g_defrostParams.humidityThresholdRH_percent=70; g_defrostParams.useNewWrkAlrAlgorithm=1;
 g_defrostParams.debugDisableDeviceSwitchCheck=checks?0:1;
 gateActive=0; returnRhDeci=850; returnRhUsable=true;
}
int main() {
 // Normal travel may keep Air_Close=1 right until the configured deadline.
 for (uint16_t timeout : {0,1,30,160,300,65535}) {
  reset(timeout); Model::DFR.Water_Flap=1;
  for (unsigned t=1;t<timeout;++t) {
   DetectFlapAlarm1s(); DetectFlapAlarmAfterIoRead();
   CHECK(!g.flapAlarm && !(Model::Device_AlarmFlags & (1u<<11)));
  }
  Model::DI_DFR.Bits.Air_Close=0;
  DetectFlapAlarm1s(); DetectFlapAlarmAfterIoRead(); CHECK(!g.flapAlarm);
  reset(timeout); Model::DFR.Water_Flap=1;
  for (unsigned t=1;t<timeout;++t) DetectFlapAlarm1s();
  DetectFlapAlarm1s(); CHECK(g.flapAlarm && (Model::Device_AlarmFlags & (1u<<11)));
 }
 // Modbus must enforce the same live parameter, never the old fixed deadline.
 reset(160); Model::DFR.Water_Flap=1; g.flapTransitionElapsed_s=159;
 DetectFlapAlarmAfterIoRead(); CHECK(!g.flapAlarm);
 g.flapTransitionElapsed_s=160; DetectFlapAlarmAfterIoRead(); CHECK(g.flapAlarm);
 // Manual command selection and disabled checks retain their semantics.
 reset(160); Model::Flag_DFR_manual=1; Model::DFR_manual.Water_Flap=1;
 for(int t=0;t<159;++t) DetectFlapAlarm1s(); CHECK(!g.flapAlarm);
 DetectFlapAlarm1s(); CHECK(g.flapAlarm);
 reset(160,false); Model::DFR.Water_Flap=1;
 for(int t=0;t<200;++t) DetectFlapAlarm1s(); CHECK(!g.flapAlarm);
 // Replay the whole stop with staged power loads and a slowly opening flap.
 reset(160);
 g.stagedTen1LeftOn=g.stagedTen2LeftOn=g.stagedTen1RightOn=g.stagedTen2RightOn=1;
 g.stagedVent1LeftOn=g.stagedVent2LeftOn=g.stagedVent1RightOn=g.stagedVent2RightOn=1;
 ShutdownSequence();
 for(int t=1;t<160;++t) {
  DetectFlapAlarm1s(); DetectFlapAlarmAfterIoRead(); ProcessShutdownStage1s();
  CHECK(!g.flapAlarm && !Model::DFR._Out);
  CHECK(g.shutdownStage!=(uint8_t)ShutdownStage::FullGateOpen);
 }
 CHECK(g.shutdownStage==(uint8_t)ShutdownStage::Ventilation);
 Model::DI_DFR.Bits.Air_Close=0;
 DetectFlapAlarm1s(); ProcessShutdownStage1s(); CHECK(Model::DFR._Out);
 ProcessShutdownStage1s(); CHECK(g.outFanOn);
 returnRhDeci=699; ProcessShutdownStage1s();
 CHECK(g.shutdownStage==(uint8_t)ShutdownStage::FullGateOpen && !Model::DFR._Out);
 // Shutdown entered after earlier stop actions: elapsed travel is already 15 s.
 reset(160); Model::DFR.Water_Flap=1; g.flapDesiredOpenPrevValid=1;
 g.flapDesiredOpenPrev=1; g.flapTransitionElapsed_s=15;
 g.shutdownActive=1; g.shutdownStage=(uint8_t)ShutdownStage::Ventilation;
 for(int t=16;t<160;++t) {
  DetectFlapAlarm1s(); ProcessShutdownStage1s();
  CHECK(g.shutdownStage==(uint8_t)ShutdownStage::Ventilation);
  CHECK(!Model::DFR._Out && !g.outFanOn && !g.flapAlarm);
 }
 Model::DI_DFR.Bits.Air_Close=0;
 DetectFlapAlarm1s(); ProcessShutdownStage1s(); CHECK(Model::DFR._Out);
 ProcessShutdownStage1s(); CHECK(g.outFanOn && g.shutdownOutFanRemain_s==299);
 Model::DI_DFR.Bits.Air_Close=1;
 DetectFlapAlarm1s(); ProcessShutdownStage1s(); CHECK(!Model::DFR._Out && !g.outFanOn);
 // Missing confirmation: skip purge at the configured timeout, with/without checks.
 for(bool checks : {true,false}) {
  reset(160,checks); Model::DFR.Water_Flap=1;
  g.shutdownActive=1; g.shutdownStage=(uint8_t)ShutdownStage::Ventilation;
  for(int t=1;t<160;++t) {
   DetectFlapAlarm1s(); ProcessShutdownStage1s();
   CHECK(g.shutdownStage==(uint8_t)ShutdownStage::Ventilation && !Model::DFR._Out);
  }
  DetectFlapAlarm1s(); ProcessShutdownStage1s();
  if (!checks) ProcessShutdownStage1s(); // fallback checks elapsed before increment
  CHECK(g.shutdownStage==(uint8_t)ShutdownStage::FullGateOpen);
  CHECK(!Model::DFR._Out && !Model::DFR.Water_Flap);
 }
 // Humidity control cannot assume opening on elapsed time, even with checks disabled.
 for(bool checks : {true,false}) {
  reset(160,checks);
  for(int t=0;t<200;++t) { StepExhaustByHumidityError(-.01f); CHECK(!g.outOn); }
  Model::DI_DFR.Bits.Air_Close=0;
  for(int t=0;t<8;++t) StepExhaustByHumidityError(-.01f);
  CHECK(g.outOn);
  Model::DI_DFR.Bits.Air_Close=1; StepExhaustByHumidityError(-.01f); CHECK(!g.outOn);
 }
 // Closing wait without device checks also uses the travel parameter.
 reset(160,false); g.shutdownStage=(uint8_t)ShutdownStage::FullGateOpen;
 g.shutdownGateFullOpenActive=1; g.shutdownGateOpenReleased=1;
 for(int t=1;t<160;++t) { ProcessShutdownStage1s(); CHECK(g.shutdownGateFullOpenActive); }
 ProcessShutdownStage1s(); CHECK(!g.shutdownGateFullOpenActive);
 // A dry return skips both opening and waiting, for either diagnostics setting.
 for(bool checks : {true,false}) {
  reset(160,checks); returnRhDeci=699;
  ShutdownSequence(); CHECK(!Model::DFR.Water_Flap && !Model::DFR._Out);
  g.shutdownStage=(uint8_t)ShutdownStage::Ventilation;
  ProcessShutdownStage1s();
  CHECK(g.shutdownStage==(uint8_t)ShutdownStage::FullGateOpen);
  CHECK(!Model::DFR.Water_Flap && !Model::DFR._Out && !g.shutdownFlapOpenWait_s);
 }
 reset(160); returnRhDeci=700; ShutdownSequence(); CHECK(Model::DFR.Water_Flap);
 reset(160); returnRhUsable=false; ShutdownSequence(); CHECK(Model::DFR.Water_Flap);
 // Without RH, a confirmed open flap permits the configured timed purge.
 g.shutdownStage=(uint8_t)ShutdownStage::Ventilation; Model::DI_DFR.Bits.Air_Close=0;
 ProcessShutdownStage1s(); CHECK(Model::DFR._Out);
 for(int t=0;t<299;++t) ProcessShutdownStage1s();
 CHECK(Model::DFR._Out && g.shutdownOutFanRemain_s==1);
 ProcessShutdownStage1s(); CHECK(!Model::DFR._Out && g.shutdownStage==(uint8_t)ShutdownStage::FullGateOpen);
 // If RH falls during travel, do not wait for opening or turn on the fan.
 reset(160); ShutdownSequence(); g.shutdownStage=(uint8_t)ShutdownStage::Ventilation;
 ProcessShutdownStage1s(); returnRhDeci=650; ProcessShutdownStage1s();
 CHECK(g.shutdownStage==(uint8_t)ShutdownStage::FullGateOpen && !Model::DFR._Out);
 // Final automatic output also rejects a stale fan request on Air_Close=1.
 reset(160); Model::DFR.Water_Flap=1;
 ApplyOutputs(0,0,0,0,0,0,0,1); CHECK(!Model::DFR._Out);
 Model::DI_DFR.Bits.Air_Close=0;
 ApplyOutputs(0,0,0,0,0,0,0,1); CHECK(Model::DFR._Out);
 Model::DI_DFR.Bits.Air_Close=1;
 ApplyOutputs(0,0,0,0,0,0,0,1); CHECK(!Model::DFR._Out);
 std::puts("PASS: parameterized flap timeout, Modbus, shutdown purge and Air_Close interlock");
}
'''


def main():
    control = (ROOT / "STM32CubeIDE/ProjectCode/DefrostControl.cpp").read_text(encoding="utf-8")
    modbus = (ROOT / "STM32CubeIDE/ProjectCode/ModBus.cpp").read_text(encoding="utf-8")
    signatures = [
        "uint8_t DefrostControl_IsDeviceSwitchCheckEnabled(void)",
        "void DefrostControl_NotifyFlapWaterDiMismatchFromIo(void)",
        "uint16_t DefrostControl_GetFlapTransitionElapsedSForSwitchCheck(void)",
    ]
    if "uint16_t DefrostControl_GetFlapPositionTimeoutS(void)" in control:
        signatures.append("uint16_t DefrostControl_GetFlapPositionTimeoutS(void)")
    signatures += [
        "static void DetectFlapAlarm1s()", "static void ApplyOutputs(",
        "static void StepExhaustByHumidityError(float wErr)",
        "static void ShutdownEnterFullGateOpenArm()", "static void ShutdownGoToFullGateOpen_VentCleanup()",
        "static uint8_t ShouldSkipShutdownVentilation(void)", "static void DetectFlapAlarmDuringShutdown()",
        "static void HandleFlapAlarmDuringShutdown()", "static uint8_t ShutdownVentilationNeeded(void)",
        "static void ShutdownSequence()", "static void ProcessShutdownStage1s()",
    ]
    source = PREFIX + "\n#include <initializer_list>\n" + "".join(function(control,s) for s in signatures)
    source += function(modbus, "static void DetectFlapAlarmAfterIoRead(void)") + SUFFIX
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise SystemExit("Put a C++14 host compiler (cl/g++/clang++) on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_flap_") as folder:
        folder=Path(folder); cpp=folder/"test.cpp"; exe=folder/"test.exe"
        cpp.write_text(source,encoding="utf-8")
        includes=str(ROOT / "STM32CubeIDE/ProjectCode")
        if Path(compiler).stem.lower()=="cl":
            args=[compiler,"/nologo","/EHsc","/std:c++14","/utf-8","/I"+includes,str(cpp),"/Fe"+str(exe)]
        else:
            args=[compiler,"-std=c++14","-I",includes,str(cpp),"-o",str(exe)]
        subprocess.run(args,cwd=folder,check=True)
        subprocess.run([str(exe)],cwd=folder,check=True)


if __name__ == "__main__":
    main()
