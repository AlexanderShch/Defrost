#include <gui/home_screen/HomeView.hpp>
#include <gui/model/Model.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/TypedText.hpp>
#include <texts/TextKeysAndLanguages.hpp>
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

/** Мигание красным ~1 Гц по секундному тику контроллера. */
inline bool ProductTAlarmBlinkOn()
{
	return (TimeFromStart % 2u) == 0u;
}

inline touchgfx::colortype ProductTempColor(int sensorIndex, bool noData)
{
	if (IsProductSensorInactiveAlarm(sensorIndex) || noData)
	{
		return kColorAlarm;
	}
	if (Model::isProductTTransitionAlarm(sensorIndex) && ProductTAlarmBlinkOn())
	{
		return kColorAlarm;
	}
	return kColorNormal;
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

    // Счётчик времени программы: заголовок и HH:MM:SS задаёт presenter.
    ValuePRGTimeWrk.setWildcard(ValuePRGTimeWrkBuffer);
    ValuePRGTimeWrk.setPosition(0, 111, 240, 24);
    updateProgramTimeView(false, 0);

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
	const bool noData = (Val == kNoDataMarker);
	if (IsProductSensorInactiveAlarm(3) || noData)
	{
		Unicode::snprintf(ValueCoreT1Buffer, sizeof(ValueCoreT1Buffer), "--");
	}
	else
	{
		Unicode::snprintfFloat(ValueCoreT1Buffer, sizeof(ValueCoreT1Buffer), "%.1f", (float)Val/10);
	}
	ValueCoreT1.setColor(ProductTempColor(3, noData));
	ValueCoreT1.invalidate();
}

// 4 - fish right temperature
void HomeView::Val_T_4UpdateView(int Val)
{
	const bool noData = (Val == kNoDataMarker);
	if (IsProductSensorInactiveAlarm(4) || noData)
	{
		Unicode::snprintf(ValueCoreT2Buffer, sizeof(ValueCoreT2Buffer), "--");
	}
	else
	{
		Unicode::snprintfFloat(ValueCoreT2Buffer, sizeof(ValueCoreT2Buffer), "%.1f", (float)Val/10);
	}
	ValueCoreT2.setColor(ProductTempColor(4, noData));
	ValueCoreT2.invalidate();
}

void HomeView::updateProgramTimeView(bool airOnly, uint32_t seconds)
{
    const touchgfx::TypedText title(
        airOnly ? T_LABELPRGTIMELEFT : T_LABELPRGTIMEELAPSED);

    // «Отработано» шире «Осталось»: invalidate() после сжатия покрывает только новый прямоугольник,
    // хвост старых букв остаётся. Помечаем объединение старого и нового, плюс фон BoxPRGTime.
    touchgfx::Rect dirtyTitle = LabelPRGTimeLeft.getRect();
    LabelPRGTimeLeft.setTypedText(title);
    LabelPRGTimeLeft.resizeToCurrentText();
    LabelPRGTimeLeft.setX((240 - LabelPRGTimeLeft.getWidth()) / 2);
    dirtyTitle.expandToFit(LabelPRGTimeLeft.getRect());
    invalidateRect(dirtyTitle);
    BoxPRGTime.invalidate();

    const uint32_t hours = (seconds / 3600u) % 100u;
    const uint32_t minutes = (seconds / 60u) % 60u;
    const uint32_t secs = seconds % 60u;

    Unicode::snprintf(ValuePRGTimeWrkBuffer, PRGTIMEWRK_SIZE, "%02u:%02u:%02u",
                      static_cast<unsigned int>(hours),
                      static_cast<unsigned int>(minutes),
                      static_cast<unsigned int>(secs));

    touchgfx::Rect dirtyTime = ValuePRGTimeWrk.getRect();
    ValuePRGTimeWrk.resizeToCurrentText();
    ValuePRGTimeWrk.setX((240 - ValuePRGTimeWrk.getWidth()) / 2);
    dirtyTime.expandToFit(ValuePRGTimeWrk.getRect());
    invalidateRect(dirtyTime);
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
