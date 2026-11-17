"""Проверка реальных функций классификации продукта с имитацией датчиков и обрывов связи."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys

# Импортируем общий извлекатель функций без создания служебных файлов в репозитории.
sys.dont_write_bytecode = True

from test_flap_timeout import function

ROOT = Path(__file__).resolve().parents[1]
PREFIX = r'''
#include <cstdint>
#include <cstdio>
constexpr int kSensSupLeft_T_H=0, kSensSupRight_T_H=1, kSensReturn_T_H=2;
constexpr int kSensFish1_T=3, kSensFish2_T=4;
constexpr uint16_t kProductFallenLeftBit=1u<<13, kProductFallenRightBit=1u<<14;
constexpr uint16_t kProductTDownLeftBit=1u<<8, kProductTDownRightBit=1u<<9;
constexpr float kProductVsAirClose_C=2, kProductFalloutRise_Cpm=2;
struct { uint8_t Active=1, UseInDefrost=1; } Sensor_array[5];
namespace Model { uint16_t Device_AlarmFlags=0, Sensor_AlarmFlags=0; }
struct State {
 uint8_t productFallen[2]={1,1}, productCoolRecover[2]={0,0}, productClassified[2]={0,0};
} g;
float temperature[5]={24,24,24,-3,-1};
float rise[5]={};
int8_t direction[5]={};
namespace Sensor {
 int8_t GetProductThermalChaseDir(unsigned char n) { return direction[n]; }
 uint8_t IsProductTransitionRateBelowNoise(unsigned char n) { return direction[n]==0; }
 uint8_t IsProductThermalTransient(unsigned char n) { return direction[n]!=0; }
}
bool SensorExcludedByClampAlarm(int n) { return (Model::Sensor_AlarmFlags & (1u<<n))!=0; }
bool DefrostAirThChannelUsable(int n) {
 return Sensor_array[n].Active && Sensor_array[n].UseInDefrost && !SensorExcludedByClampAlarm(n);
}
float FilteredSensorT_C(int n) { return temperature[n]; }
float FilteredSensorRise_Cpm(int n) { return rise[n]; }
uint8_t DefrostControl_IsEnabled() { return 1; }
#define CHECK(x) do { if(!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
'''
SUFFIX = r'''
int main() {
 // Холодные зонды при запуске распознаются как установленные в продукт.
 DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==3);
 // Длительная потеря всех воздушных каналов не создаёт ложное выпадение зондов.
 for(int n=0;n<3;++n) Sensor_array[n].Active=0;
 for(int tick=0;tick<120;++tick) {
  DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==3);
  CHECK(!(Model::Device_AlarmFlags&(kProductFallenLeftBit|kProductFallenRightBit)));
 }
 // Зонд возвращается после обрыва даже при отсутствии воздушной опоры.
 Sensor_array[3].Active=0; DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==2);
 CHECK(!g.productFallen[0] && g.productClassified[0]);
 Sensor_array[3].Active=1; DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==3);
 for(int n=0;n<3;++n) Sensor_array[n].Active=1;
 DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==3);
 // После восстановления шины оба зонда возвращаются без требования охлаждения.
 Sensor_array[3].Active=Sensor_array[4].Active=0;
 DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==0);
 Sensor_array[3].Active=Sensor_array[4].Active=1;
 DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==3);
 // Настоящий положительный переход температуры по-прежнему фиксирует выпадение.
 direction[3]=1; DetectSensorAlarms1s(); CHECK(g.productFallen[0]);
 CHECK(Model::Device_AlarmFlags&kProductFallenLeftBit);
 direction[3]=0; Sensor_array[3].Active=0; DetectSensorAlarms1s();
 Sensor_array[3].Active=1; DetectSensorAlarms1s();
 CHECK(g.productFallen[0] && HandleSensorAlarmsForControl()==2);
 // Подтверждённое выпадение снимается по прежнему критерию охлаждения.
 direction[3]=-1; DetectSensorAlarms1s(); direction[3]=0; DetectSensorAlarms1s();
 CHECK(HandleSensorAlarmsForControl()==3);
 // При запуске без воздуха классификация ожидает данных, авария выпадения не возникает.
 g=State{}; for(int n=0;n<3;++n) Sensor_array[n].Active=0;
 DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==0);
 CHECK(!(Model::Device_AlarmFlags&(kProductFallenLeftBit|kProductFallenRightBit)));
 Sensor_array[1].Active=1; DetectSensorAlarms1s(); CHECK(HandleSensorAlarmsForControl()==3);
 // Тёплый зонд при запуске по-прежнему распознаётся как находящийся вне продукта.
 g=State{}; temperature[3]=24; DetectSensorAlarms1s();
 CHECK(g.productFallen[0] && HandleSensorAlarmsForControl()==2);
 std::puts("PASS: air loss, product bus recovery, pending startup and genuine fallout protection");
}
'''


def main():
    """Извлекает рабочие функции, компилирует временный стенд и проверяет код возврата.

    Параметров нет; при ошибке компиляции или проверки возбуждается исключение.
    Контроллер и его физические выходы не используются.
    """
    source = (ROOT / "STM32CubeIDE/ProjectCode/DefrostControl.cpp").read_text(encoding="utf-8")
    source = PREFIX + function(source, "static void DetectSensorAlarms1s(void)") + function(
        source, "static uint8_t HandleSensorAlarmsForControl()") + SUFFIX
    compiler = shutil.which("cl") or shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise SystemExit("Put a C++14 host compiler (cl/g++/clang++) on PATH.")
    with tempfile.TemporaryDirectory(prefix="defrost_product_") as folder:
        folder = Path(folder)
        cpp, exe = folder / "test.cpp", folder / "test.exe"
        cpp.write_text(source, encoding="utf-8")
        if Path(compiler).name.lower() == "cl.exe":
            args = [compiler, "/nologo", "/EHsc", "/std:c++14", "/utf-8", str(cpp), "/Fe:" + str(exe)]
        else:
            args = [compiler, "-std=c++14", str(cpp), "-o", str(exe)]
        subprocess.run(args, cwd=folder, check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
