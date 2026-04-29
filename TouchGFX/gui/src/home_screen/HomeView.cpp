#include <gui/home_screen/HomeView.hpp>
#include <gui/model/Model.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/Color.hpp>
#include "DefrostControl.h"
#include "ModBus.hpp"

extern SENSOR_typedef_t Sensor_array[SQ];

namespace
{
constexpr int kNoDataMarker = -32768;
const touchgfx::colortype kColorNormal = touchgfx::Color::getColorFromRGB(232, 246, 251);
const touchgfx::colortype kColorAlarm = touchgfx::Color::getColorFromRGB(255, 0, 0);

inline bool IsProductSensorInactiveAlarm(int sensorIndex)
{
	if (sensorIndex < 0 || sensorIndex >= SQ)
	{
		return false;
	}
	return (Sensor_array[sensorIndex].Active == 0u) && ((Model::Sensor_AlarmFlags & (1u << sensorIndex)) != 0u);
}
}

HomeView::HomeView()
{

}

void HomeView::setupScreen()
{
    HomeViewBase::setupScreen();

    // Привязка нажатия BTNStart к запуску алгоритма разморозки.
    startButtonCallback = touchgfx::Callback<HomeView, const touchgfx::AbstractButton&>(this, &HomeView::onBTNStartClicked);
    BTNStart.setAction(startButtonCallback);
    // ToggleButton: getState() 0 = Released (OFF), 1 = Pressed (ON). Синхронизируем с DefrostControl.
    BTNStart.forceState(DefrostControl_IsEnabled() != 0 ? 1 : 0);
    BTNStart.invalidate();

    // Счётчик отработанного времени (ValuePRGTimeWrk): подключаем свой буфер и выводим 00:00:00.
    ValuePRGTimeWrk.setWildcard(ValuePRGTimeWrkBuffer);
    ValuePRGTimeWrk.setPosition(0, 111, 240, 24);
    updateProgramRuntimeView(0);

    // Устанавливаем версию прошивки при инициализации экрана
    updateVersionDisplay();

    // В сгенерированном Base видимость по умолчанию может отличаться от Designer — скрываем до появления _Alr.
    textArea_Alarm.setVisible(false);
    textArea_Alarm.invalidate();
}

void HomeView::onBTNStartClicked(const touchgfx::AbstractButton&)
{
    // getState(): 0 = Released (OFF/Стоп), 1 = Pressed (ON/Старт). Колбэк вызывается при отпускании.
    // getState() == 1 (Pressed, ON) → startDefrostRequested()
    // getState() == 0 (Released, OFF) → stopDefrostRequested()
    if (BTNStart.getState() == 1)
        presenter->startDefrostRequested();
    else
        presenter->stopDefrostRequested();
}

void HomeView::tearDownScreen()
{
    HomeViewBase::tearDownScreen();
}

// 3 - fish left temperature
void HomeView::Val_T_3UpdateView(int Val)
{
	if (IsProductSensorInactiveAlarm(3) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueCoreT1Buffer, sizeof(ValueCoreT1Buffer), "--");
		ValueCoreT1.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueCoreT1Buffer, sizeof(ValueCoreT1Buffer), "%.1f", (float)Val/10);
		ValueCoreT1.setColor(kColorNormal);
	}
	ValueCoreT1.invalidate();
}

// 4 - fish right temperature
void HomeView::Val_T_4UpdateView(int Val)
{
	if (IsProductSensorInactiveAlarm(4) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueCoreT2Buffer, sizeof(ValueCoreT2Buffer), "--");
		ValueCoreT2.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueCoreT2Buffer, sizeof(ValueCoreT2Buffer), "%.1f", (float)Val/10);
		ValueCoreT2.setColor(kColorNormal);
	}
	ValueCoreT2.invalidate();
}

void HomeView::updateProgramRuntimeView(uint32_t runtimeSeconds)
{
    const uint32_t hours = (runtimeSeconds / 3600u) % 100u;
    const uint32_t minutes = (runtimeSeconds / 60u) % 60u;
    const uint32_t seconds = runtimeSeconds % 60u;

    Unicode::snprintf(ValuePRGTimeWrkBuffer, PRGTIMEWRK_SIZE, "%02u:%02u:%02u",
                      static_cast<unsigned int>(hours),
                      static_cast<unsigned int>(minutes),
                      static_cast<unsigned int>(seconds));

    ValuePRGTimeWrk.resizeToCurrentText();
    ValuePRGTimeWrk.setX((240 - ValuePRGTimeWrk.getWidth()) / 2);
    ValuePRGTimeWrk.invalidate();
}

/*
 * Функция: updateVersionDisplay
 * Описание: Устанавливает текущую версию прошивки в wildcard VersionValue
 * 
 * Почему: используем сгенерированные LabelVersion/LabelVersionBuffer из TouchGFX Designer.
 */
void HomeView::syncStartButtonState()
{
    const uint8_t enabled = DefrostControl_IsEnabled() != 0 ? 1 : 0;
    if (BTNStart.getState() != enabled)
    {
        BTNStart.forceState(enabled);
        BTNStart.invalidate();
    }
}

void HomeView::updateAlarmBanner(bool alarmActive)
{
    textArea_Alarm.setVisible(alarmActive);
    textArea_Alarm.invalidate();
}

void HomeView::updateVersionDisplay()
{
    // Получаем версию прошивки из Model
    const char* version = Model::getFirmwareVersion();
    
    // Преобразуем строку версии в Unicode и записываем в буфер wildcard.
    Unicode::strncpy(LabelVersionBuffer, version, LABELVERSION_SIZE);
    LabelVersionBuffer[LABELVERSION_SIZE - 1] = 0;

    // Обновляем отображение на экране.
    LabelVersion.invalidate();
}
