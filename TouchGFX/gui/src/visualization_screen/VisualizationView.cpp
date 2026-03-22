#include <gui/visualization_screen/VisualizationView.hpp>
#include <gui/model/Model.hpp>
#include <touchgfx/utils.hpp>
#include <touchgfx/Color.hpp>

//#include "string.h"
#include "stdio.h"

bool val = false;


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
	// В Base анимация вытяжки стартует в конструкторе — останавливаем до синхронизации с DI.
	if (AnimFan_Out.isAnimatedImageRunning())
		AnimFan_Out.pauseAnimation();
	AnimFan_Out.invalidate();
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
	Unicode::snprintfFloat(ValueDefrosterT1Buffer, sizeof(ValueDefrosterT1Buffer), "%.1f", (float)Val/10);
	ValueDefrosterT1.invalidate();
}

// 1 - defroster right temperature
void VisualizationView::Val_T_1UpdateView(short Val)
{
	Unicode::snprintfFloat(ValueDefrosterT2Buffer, sizeof(ValueDefrosterT2Buffer), "%.1f", (float)Val/10);
	ValueDefrosterT2.invalidate();
}

// 2 - defroster center temperature
void VisualizationView::Val_T_2UpdateView(short Val)
{
	Unicode::snprintfFloat(ValueDefrosterTBuffer, sizeof(ValueDefrosterTBuffer), "%.1f", (float)Val/10);
	ValueDefrosterT.invalidate();
}

// 3 - fish left temperature
void VisualizationView::Val_T_3UpdateView(short Val)
{
	Unicode::snprintfFloat(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "%.1f", (float)Val/10);
	ValueCoreT1Small.invalidate();
}

// 4 - fish right temperature
void VisualizationView::Val_T_4UpdateView(short Val)
{
	Unicode::snprintfFloat(ValueCoreT2SmallBuffer, sizeof(ValueCoreT2SmallBuffer), "%.1f", (float)Val/10);
	ValueCoreT2Small.invalidate();
}

// Humidity
// 0 - defroster left humidity
void VisualizationView::Val_H_0UpdateView(short Val)
{
	Unicode::snprintfFloat(ValueDefrosterH1Buffer, sizeof(ValueDefrosterH1Buffer), "%.1f", (float)Val/10);
	ValueDefrosterH1.invalidate();
}

// 1 - defroster right humidity
void VisualizationView::Val_H_1UpdateView(short Val)
{
	Unicode::snprintfFloat(ValueDefrosterH2Buffer, sizeof(ValueDefrosterH2Buffer), "%.1f", (float)Val/10);
	ValueDefrosterH2.invalidate();
}

// 2 - defroster center humidity
void VisualizationView::Val_H_2UpdateView(short Val)
{
	Unicode::snprintfFloat(ValueDefrosterHBuffer, sizeof(ValueDefrosterHBuffer), "%.1f", (float)Val/10);
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
	if ((Model::DFR_current.Ten1_Left || Model::DFR_current.Ten2_Left) == 1) val = true; else val = false;
	// Если один из тэнов или оба включены, то видимость работы тэна включена
	AnimHeat12.setVisible(val);

	if (val) {
		if (!(AnimHeat12.isAnimatedImageRunning()))
			AnimHeat12.startAnimation(false, false, true);
		if ((Model::DFR_current.Ten1_Left ^ Model::DFR_current.Ten2_Left) == 1) {
			AnimHeat12.setUpdateTicksInterval(8);
		}
		else {
			AnimHeat12.setUpdateTicksInterval(3);
		}
	}
	else {
		if (AnimHeat12.isAnimatedImageRunning())
			AnimHeat12.pauseAnimation();
	}
	AnimHeat12.invalidate();
}
void VisualizationView::AnimHeat34_Switch()
{
	if ((Model::DFR_current.Ten1_Right || Model::DFR_current.Ten2_Right) == 1) val = true; else val = false;
	// Если один из тэнов или оба включены, то видимость работы тэна включена
	// а если оба тэна выключены, то видимость погашена
	AnimHeat34.setVisible(val);

	if (val) {
		if (!(AnimHeat34.isAnimatedImageRunning()))
			AnimHeat34.startAnimation(false, false, true);
		if ((Model::DFR_current.Ten1_Right ^ Model::DFR_current.Ten2_Right) == 1) {
			AnimHeat34.setUpdateTicksInterval(8);
		}
		else {
			AnimHeat34.setUpdateTicksInterval(3);
		}
	}
	else {
		if (AnimHeat34.isAnimatedImageRunning())
			AnimHeat34.pauseAnimation();
	}
	AnimHeat34.invalidate();
}
void VisualizationView::AnimFan12_Switch()
{
	if ((Model::DFR_current.Vent1_Left || Model::DFR_current.Vent2_Left) == 1) {
		// Если один из вентиляторов или оба включены, то видимость вращения включена
		// Если вентилятор был остановлен, то запустим его
		if (!(AnimFan12.isAnimatedImageRunning()))
			AnimFan12.startAnimation(false, false, true);
		if ((Model::DFR_current.Vent1_Left ^ Model::DFR_current.Vent2_Left) == 1) {
			// Если включен только один вентилятор, то скорость вращения маленькая
			AnimFan12.setUpdateTicksInterval(8);
		}
		else {
			// Если включены два вентилятора, то скорость вращения большая
			AnimFan12.setUpdateTicksInterval(3);
		}
	}else {
		// Все вентиляторы выключены, но анимация работает, вентилятор остановим
		if (AnimFan12.isAnimatedImageRunning())
			AnimFan12.pauseAnimation();
	};

	AnimFan12.invalidate();
}
void VisualizationView::AnimFan34_Switch()
{
	if ((Model::DFR_current.Vent1_Right || Model::DFR_current.Vent2_Right) == 1) {
		// Если один из вентиляторов или оба включены, то видимость вращения включена
		// Если вентилятор был остановлен, то запустим его
		if (!(AnimFan34.isAnimatedImageRunning()))
			AnimFan34.startAnimation(false, false, true);
		if ((Model::DFR_current.Vent1_Right ^ Model::DFR_current.Vent2_Right) == 1) {
			// Если включен только один вентилятор, то скорость вращения маленькая
			AnimFan34.setUpdateTicksInterval(8);
		}
		else {
			// Если включены два вентилятора, то скорость вращения большая
			AnimFan34.setUpdateTicksInterval(3);
		}
	}else {
		// Все вентиляторы выключены, но анимация работает, вентилятор остановим
		if (AnimFan34.isAnimatedImageRunning())
			AnimFan34.pauseAnimation();
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

	// Авария подтверждения вытяжки (рассогласование выход/вход по Vent_Out)
	const bool ventOutAlarm = (af & (1u << 8)) != 0;
	Vent_Out_Alarm.setVisible(ventOutAlarm);
	Vent_Out_Alarm.invalidate();
}

void VisualizationView::syncExhaustFanAndFlapFromInputs()
{
	// Входы с модуля DI (см. Model.hpp DI_DFR_REGISTERS_t).
	const uint8_t ventOut = (uint8_t)Model::DI_DFR.Bits.Vent_Out;
	if (ventOut != 0u)
	{
		if (!AnimFan_Out.isAnimatedImageRunning())
			AnimFan_Out.startAnimation(false, false, true);
		AnimFan_Out.setUpdateTicksInterval(3);
	}
	else
	{
		if (AnimFan_Out.isAnimatedImageRunning())
			AnimFan_Out.pauseAnimation();
	}
	AnimFan_Out.invalidate();

	const uint8_t airOpen = (uint8_t)Model::DI_DFR.Bits.Air_Open;
	const uint8_t airClose = (uint8_t)Model::DI_DFR.Bits.Air_Close;

	bool showFlap = false;
	bool showOpen = false;
	bool showClose = false;

	if (airOpen == 0u && airClose == 0u)
		showFlap = true;
	else if (airOpen != 0u && airClose == 0u)
		showOpen = true;
	else if (airOpen == 0u && airClose != 0u)
		showClose = true;
	// Оба в 1 — противоречие датчиков: ни один виджет не показываем.

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

