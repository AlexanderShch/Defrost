#include <gui/visualization_screen/VisualizationView.hpp>
#include <gui/common/AirFlapState.hpp>
#include <gui/common/ExhaustOutputState.hpp>
#include <gui/model/Model.hpp>
#include <touchgfx/utils.hpp>
#include <touchgfx/Color.hpp>
#include "ModBus.hpp"

//#include "string.h"
#include "stdio.h"

extern uint16_t RelayRegister;
extern SENSOR_typedef_t Sensor_array[SQ];

bool val = false;

namespace
{
constexpr short kNoDataMarker = (short)-32768;
const touchgfx::colortype kColorNormal = touchgfx::Color::getColorFromRGB(232, 246, 251);
const touchgfx::colortype kColorAlarm = touchgfx::Color::getColorFromRGB(255, 0, 0);

inline bool IsSensorInactiveAlarm(int sensorIndex)
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
	if (IsSensorInactiveAlarm(sensorIndex) || noData)
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


/*	Current temperature from sensor saved in CurrentValue[SQ] array
 * 	0 - defroster left
 * 	1 - defroster right
 * 	2 - defroster center
 *	3 - fish left
 *	4 - fish right
 */


VisualizationView::VisualizationView()
{

}

void VisualizationView::setupScreen()
{
    VisualizationViewBase::setupScreen();
	// Анимация вытяжки должна работать непрерывно (не останавливаем в setup).
	syncExhaustFanAndFlapFromInputs();
	// Подтянуть анимации к текущему DFR_current (в т.ч. если флаги смены не сработали).
	syncEquipmentAnimationsFromDisplayedState();
}

void VisualizationView::tearDownScreen()
{
    VisualizationViewBase::tearDownScreen();
}

// Temperature
// 0 - defroster left temperature
void VisualizationView::Val_T_0UpdateView(short Val)
{
	if (IsSensorInactiveAlarm(0) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueDefrosterT1Buffer, sizeof(ValueDefrosterT1Buffer), "--");
		ValueDefrosterT1.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueDefrosterT1Buffer, sizeof(ValueDefrosterT1Buffer), "%.1f", (float)Val/10);
		ValueDefrosterT1.setColor(kColorNormal);
	}
	ValueDefrosterT1.invalidate();
}

// 1 - defroster right temperature
void VisualizationView::Val_T_1UpdateView(short Val)
{
	if (IsSensorInactiveAlarm(1) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueDefrosterT2Buffer, sizeof(ValueDefrosterT2Buffer), "--");
		ValueDefrosterT2.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueDefrosterT2Buffer, sizeof(ValueDefrosterT2Buffer), "%.1f", (float)Val/10);
		ValueDefrosterT2.setColor(kColorNormal);
	}
	ValueDefrosterT2.invalidate();
}

// 2 - defroster center temperature
void VisualizationView::Val_T_2UpdateView(short Val)
{
	if (IsSensorInactiveAlarm(2) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueDefrosterTBuffer, sizeof(ValueDefrosterTBuffer), "--");
		ValueDefrosterT.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueDefrosterTBuffer, sizeof(ValueDefrosterTBuffer), "%.1f", (float)Val/10);
		ValueDefrosterT.setColor(kColorNormal);
	}
	ValueDefrosterT.invalidate();
}

// 3 - fish left temperature
void VisualizationView::Val_T_3UpdateView(short Val)
{
	const bool noData = (Val == kNoDataMarker);
	if (IsSensorInactiveAlarm(3) || noData)
	{
		Unicode::snprintf(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "--");
	}
	else
	{
		Unicode::snprintfFloat(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "%.1f", (float)Val/10);
	}
	ValueCoreT1Small.setColor(ProductTempColor(3, noData));
	ValueCoreT1Small.invalidate();
}

// 4 - fish right temperature
void VisualizationView::Val_T_4UpdateView(short Val)
{
	const bool noData = (Val == kNoDataMarker);
	if (IsSensorInactiveAlarm(4) || noData)
	{
		Unicode::snprintf(ValueCoreT2SmallBuffer, sizeof(ValueCoreT2SmallBuffer), "--");
	}
	else
	{
		Unicode::snprintfFloat(ValueCoreT2SmallBuffer, sizeof(ValueCoreT2SmallBuffer), "%.1f", (float)Val/10);
	}
	ValueCoreT2Small.setColor(ProductTempColor(4, noData));
	ValueCoreT2Small.invalidate();
}

// Humidity
// 0 - defroster left humidity
void VisualizationView::Val_H_0UpdateView(short Val)
{
	if (IsSensorInactiveAlarm(0) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueDefrosterH1Buffer, sizeof(ValueDefrosterH1Buffer), "--");
		ValueDefrosterH1.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueDefrosterH1Buffer, sizeof(ValueDefrosterH1Buffer), "%.1f", (float)Val/10);
		ValueDefrosterH1.setColor(kColorNormal);
	}
	ValueDefrosterH1.invalidate();
}

// 1 - defroster right humidity
void VisualizationView::Val_H_1UpdateView(short Val)
{
	if (IsSensorInactiveAlarm(1) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueDefrosterH2Buffer, sizeof(ValueDefrosterH2Buffer), "--");
		ValueDefrosterH2.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueDefrosterH2Buffer, sizeof(ValueDefrosterH2Buffer), "%.1f", (float)Val/10);
		ValueDefrosterH2.setColor(kColorNormal);
	}
	ValueDefrosterH2.invalidate();
}

// 2 - defroster center humidity
void VisualizationView::Val_H_2UpdateView(short Val)
{
	if (IsSensorInactiveAlarm(2) || Val == kNoDataMarker)
	{
		Unicode::snprintf(ValueDefrosterHBuffer, sizeof(ValueDefrosterHBuffer), "--");
		ValueDefrosterH.setColor(kColorAlarm);
	}
	else
	{
		Unicode::snprintfFloat(ValueDefrosterHBuffer, sizeof(ValueDefrosterHBuffer), "%.1f", (float)Val/10);
		ValueDefrosterH.setColor(kColorNormal);
	}
	ValueDefrosterH.invalidate();
}

// 3 - fish left humidity
void VisualizationView::Val_H_3UpdateView(short Val)
{
//	Unicode::snprintfFloat(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "%.1f", (float)Val/10);
//	ValueCoreT1Small.invalidate();
}

// 4 - fish right humidity
void VisualizationView::Val_H_4UpdateView(short Val)
{
//	Unicode::snprintfFloat(ValueCoreT2SmallBuffer, sizeof(ValueCoreT2SmallBuffer), "%.1f", (float)Val/10);
//	ValueCoreT2Small.invalidate();
}

/****************************************************************
 * Отображение оборудования
 ****************************************************************/
void VisualizationView::Val_Ten1_Left_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateHeat1.setVisible(val);
	StateHeat1.invalidate();
	AnimHeat12_Switch();
	Model::DFR_chng_flag.Ten1_Left = 0;
}
void VisualizationView::Val_Ten2_Left_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateHeat2.setVisible(val);
	StateHeat2.invalidate();
	AnimHeat12_Switch();
	Model::DFR_chng_flag.Ten2_Left = 0;
}
void VisualizationView::Val_Ten1_Right_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateHeat3.setVisible(val);
	StateHeat3.invalidate();
	AnimHeat34_Switch();
	Model::DFR_chng_flag.Ten1_Right = 0;
}
void VisualizationView::Val_Ten2_Right_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateHeat4.setVisible(val);
	StateHeat4.invalidate();
	AnimHeat34_Switch();
	Model::DFR_chng_flag.Ten2_Right = 0;
}
void VisualizationView::Val_Vent1_Left_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateFan1.setVisible(val);
	StateFan1.invalidate();
	AnimFan12_Switch();
	Model::DFR_chng_flag.Vent1_Left = 0;
}
void VisualizationView::Val_Vent2_Left_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateFan2.setVisible(val);
	StateFan2.invalidate();
	AnimFan12_Switch();
	Model::DFR_chng_flag.Vent2_Left = 0;
}
void VisualizationView::Val_Vent1_Right_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateFan3.setVisible(val);
	StateFan3.invalidate();
	AnimFan34_Switch();
	Model::DFR_chng_flag.Vent1_Right = 0;
}
void VisualizationView::Val_Vent2_Right_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
	StateFan4.setVisible(val);
	StateFan4.invalidate();
	AnimFan34_Switch();
	Model::DFR_chng_flag.Vent2_Right = 0;
}
void VisualizationView::Val_Water_Flap_UpdateView(uint8_t state)
{
	if (state == 1) val = true; else val = false;
//	Water_Flap.setVisible(val);
	Model::DFR_chng_flag.Water_Flap = 0;
}

void VisualizationView::AnimHeat12_Switch()
{
	static uint8_t s_heat12LastInterval = 0u;
	if ((Model::DFR_current.Ten1_Left || Model::DFR_current.Ten2_Left) == 1) val = true; else val = false;
	// Если один из тэнов или оба включены, то видимость работы тэна включена
	AnimHeat12.setVisible(val);

	if (val) {
		if (!(AnimHeat12.isAnimatedImageRunning()))
			AnimHeat12.startAnimation(false, false, true);
		const uint8_t wantedInterval = ((Model::DFR_current.Ten1_Left ^ Model::DFR_current.Ten2_Left) == 1) ? 8u : 3u;
		if (s_heat12LastInterval != wantedInterval) {
			AnimHeat12.setUpdateTicksInterval(wantedInterval);
			s_heat12LastInterval = wantedInterval;
		}
	}
	else {
		if (AnimHeat12.isAnimatedImageRunning())
			AnimHeat12.pauseAnimation();
		s_heat12LastInterval = 0u;
	}
	AnimHeat12.invalidate();
}
void VisualizationView::AnimHeat34_Switch()
{
	static uint8_t s_heat34LastInterval = 0u;
	if ((Model::DFR_current.Ten1_Right || Model::DFR_current.Ten2_Right) == 1) val = true; else val = false;
	// Если один из тэнов или оба включены, то видимость работы тэна включена
	// а если оба тэна выключены, то видимость погашена
	AnimHeat34.setVisible(val);

	if (val) {
		if (!(AnimHeat34.isAnimatedImageRunning()))
			AnimHeat34.startAnimation(false, false, true);
		const uint8_t wantedInterval = ((Model::DFR_current.Ten1_Right ^ Model::DFR_current.Ten2_Right) == 1) ? 8u : 3u;
		if (s_heat34LastInterval != wantedInterval) {
			AnimHeat34.setUpdateTicksInterval(wantedInterval);
			s_heat34LastInterval = wantedInterval;
		}
	}
	else {
		if (AnimHeat34.isAnimatedImageRunning())
			AnimHeat34.pauseAnimation();
		s_heat34LastInterval = 0u;
	}
	AnimHeat34.invalidate();
}
void VisualizationView::AnimFan12_Switch()
{
	static uint8_t s_fan12LastInterval = 0u;
	if ((Model::DFR_current.Vent1_Left || Model::DFR_current.Vent2_Left) == 1) {
		// Если один из вентиляторов или оба включены, то видимость вращения включена
		// Если вентилятор был остановлен, то запустим его
		if (!(AnimFan12.isAnimatedImageRunning()))
			AnimFan12.startAnimation(false, false, true);
		const uint8_t wantedInterval = ((Model::DFR_current.Vent1_Left ^ Model::DFR_current.Vent2_Left) == 1) ? 8u : 3u;
		if (s_fan12LastInterval != wantedInterval) {
			AnimFan12.setUpdateTicksInterval(wantedInterval);
			s_fan12LastInterval = wantedInterval;
		}
	}else {
		// Все вентиляторы выключены, но анимация работает, вентилятор остановим
		if (AnimFan12.isAnimatedImageRunning())
			AnimFan12.pauseAnimation();
		s_fan12LastInterval = 0u;
	};

	AnimFan12.invalidate();
}
void VisualizationView::AnimFan34_Switch()
{
	static uint8_t s_fan34LastInterval = 0u;
	if ((Model::DFR_current.Vent1_Right || Model::DFR_current.Vent2_Right) == 1) {
		// Если один из вентиляторов или оба включены, то видимость вращения включена
		// Если вентилятор был остановлен, то запустим его
		if (!(AnimFan34.isAnimatedImageRunning()))
			AnimFan34.startAnimation(false, false, true);
		const uint8_t wantedInterval = ((Model::DFR_current.Vent1_Right ^ Model::DFR_current.Vent2_Right) == 1) ? 8u : 3u;
		if (s_fan34LastInterval != wantedInterval) {
			AnimFan34.setUpdateTicksInterval(wantedInterval);
			s_fan34LastInterval = wantedInterval;
		}
	}else {
		// Все вентиляторы выключены, но анимация работает, вентилятор остановим
		if (AnimFan34.isAnimatedImageRunning())
			AnimFan34.pauseAnimation();
		s_fan34LastInterval = 0u;
	};
	AnimFan34.invalidate();
}

void VisualizationView::syncDeviceAlarmIndicators()
{
	const uint16_t af = Model::Device_AlarmFlags;
	const auto colorNormal = touchgfx::Color::getColorFromRGB(232, 246, 251);
	const auto colorAlarm = touchgfx::Color::getColorFromRGB(255, 0, 0); // #FF0000

	// Порядок бит совпадает с kDeviceCheckMask в ModBus.cpp: Vent1_L, Vent2_L, Vent1_R, Vent2_R, Ten1_L, Ten2_L, Ten1_R, Ten2_R; бит 8 — Vent_Out.
	struct Row
	{
		touchgfx::TextArea* widget;
		unsigned bit;
		uint8_t dfrOn;
	};
	Row rows[8] = {
		{ &StateFan1,  0u, (uint8_t)Model::DFR_current.Vent1_Left },
		{ &StateFan2,  1u, (uint8_t)Model::DFR_current.Vent2_Left },
		{ &StateFan3,  2u, (uint8_t)Model::DFR_current.Vent1_Right },
		{ &StateFan4,  3u, (uint8_t)Model::DFR_current.Vent2_Right },
		{ &StateHeat1, 4u, (uint8_t)Model::DFR_current.Ten1_Left },
		{ &StateHeat2, 5u, (uint8_t)Model::DFR_current.Ten2_Left },
		{ &StateHeat3, 6u, (uint8_t)Model::DFR_current.Ten1_Right },
		{ &StateHeat4, 7u, (uint8_t)Model::DFR_current.Ten2_Right },
	};

	for (unsigned i = 0; i < 8; ++i)
	{
		const bool alarm = (af & (1u << rows[i].bit)) != 0;
		touchgfx::TextArea& ta = *rows[i].widget;
		if (alarm)
		{
			ta.setVisible(true);
			ta.setColor(colorAlarm);
		}
		else
		{
			ta.setColor(colorNormal);
			ta.setVisible(rows[i].dfrOn != 0);
		}
		ta.invalidate();
	}

	// Мигание синхронно с общей индикацией АВАРИЯ (_Alr).
	const bool alarmBlinkOn = (Model::DFR_current._Alr != 0);

	// Авария подтверждения вытяжки (рассогласование выход/вход по Vent_Out)
	const bool ventOutAlarm = (af & (1u << 8)) != 0;
	Vent_Out_Alarm.setVisible(ventOutAlarm && alarmBlinkOn);
	Vent_Out_Alarm.invalidate();

	// Авария заслонки (бит 11)
	const bool flapAlarm = (af & (1u << 11)) != 0;
	Flap_Alarm.setVisible(flapAlarm && alarmBlinkOn);
	Flap_Alarm.invalidate();
}

void VisualizationView::syncExhaustFanAndFlapFromInputs()
{
	static uint8_t s_fanOutLastInterval = 0u;
	// Анимация вытяжного вентилятора — по выходной команде (DFR_current), а не по входу DI.
	const uint8_t ventOut = (uint8_t)Model::DFR_current._Out;
	if (IsExhaustFanOn(ventOut))
	{
		if (!AnimFan_Out.isAnimatedImageRunning())
			AnimFan_Out.startAnimation(false, false, true);
		const uint8_t wantedInterval = 3u;
		if (s_fanOutLastInterval != wantedInterval)
		{
			AnimFan_Out.setUpdateTicksInterval(wantedInterval);
			s_fanOutLastInterval = wantedInterval;
		}
	}
	else
	{
		if (AnimFan_Out.isAnimatedImageRunning())
			AnimFan_Out.pauseAnimation();
		s_fanOutLastInterval = 0u;
	}
	AnimFan_Out.invalidate();

	const AirFlapState flapState = ResolveAirFlapState(
		(uint8_t)Model::DI_DFR.Bits.Air_Open,
		(uint8_t)Model::DI_DFR.Bits.Air_Close);

	const bool showFlap = (flapState == AirFlapState::Moving);
	const bool showOpen = (flapState == AirFlapState::Open);
	const bool showClose = (flapState == AirFlapState::Closed);
	// Состояние Invalid (оба входа = 1): ни один виджет не показываем.

	Flap.setVisible(showFlap);
	Flap_Open.setVisible(showOpen);
	Flap_Close.setVisible(showClose);
	Flap.invalidate();
	Flap_Open.invalidate();
	Flap_Close.invalidate();
}
void VisualizationView::syncEquipmentAnimationsFromDisplayedState()
{
	AnimFan12_Switch();
	AnimFan34_Switch();
	AnimHeat12_Switch();
	AnimHeat34_Switch();
}

