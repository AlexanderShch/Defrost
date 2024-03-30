#include <gui/visualization_screen/VisualizationView.hpp>
#include <touchgfx/utils.hpp>

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
}

void VisualizationView::tearDownScreen()
{
    VisualizationViewBase::tearDownScreen();
}

// Temperature
// 0 - defroster left temperature
void VisualizationView::Val_T_0UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterT1Buffer, sizeof(ValueDefrosterT1Buffer), "%.1f", (float)Val/10);
	ValueDefrosterT1.invalidate();
}

// 1 - defroster right temperature
void VisualizationView::Val_T_1UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterT2Buffer, sizeof(ValueDefrosterT2Buffer), "%.1f", (float)Val/10);
	ValueDefrosterT2.invalidate();
}

// 2 - defroster center temperature
void VisualizationView::Val_T_2UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterTBuffer, sizeof(ValueDefrosterTBuffer), "%.1f", (float)Val/10);
	ValueDefrosterT.invalidate();
}

// 3 - fish left temperature
void VisualizationView::Val_T_3UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "%.1f", (float)Val/10);
	ValueCoreT1Small.invalidate();
}

// 4 - fish right temperature
void VisualizationView::Val_T_4UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT2SmallBuffer, sizeof(ValueCoreT2SmallBuffer), "%.1f", (float)Val/10);
	ValueCoreT2Small.invalidate();
}

// Humidity
// 0 - defroster left humidity
void VisualizationView::Val_H_0UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterH1Buffer, sizeof(ValueDefrosterH1Buffer), "%.1f", (float)Val/10);
	ValueDefrosterH1.invalidate();
}

// 1 - defroster right humidity
void VisualizationView::Val_H_1UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterH2Buffer, sizeof(ValueDefrosterH2Buffer), "%.1f", (float)Val/10);
	ValueDefrosterH2.invalidate();
}

// 2 - defroster center humidity
void VisualizationView::Val_H_2UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterHBuffer, sizeof(ValueDefrosterHBuffer), "%.1f", (float)Val/10);
	ValueDefrosterH.invalidate();
}

// 3 - fish left humidity
void VisualizationView::Val_H_3UpdateView(int Val)
{
//	Unicode::snprintfFloat(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "%.1f", (float)Val/10);
//	ValueCoreT1Small.invalidate();
}

// 4 - fish right humidity
void VisualizationView::Val_H_4UpdateView(int Val)
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

	if ((Model::DFR_current.Ten1_Left ^ Model::DFR_current.Ten2_Left) == 1) {
		// Если включен только один тэн, то скорость волны маленькая
		AnimHeat12.setUpdateTicksInterval(8);
	}
	else {
		// Если включены два тэна, то скорость волны большая
		AnimHeat12.setUpdateTicksInterval(3);
	}
	AnimHeat12.invalidate();
}
void VisualizationView::AnimHeat34_Switch()
{
	if ((Model::DFR_current.Ten1_Right || Model::DFR_current.Ten2_Right) == 1) val = true; else val = false;
	// Если один из тэнов или оба включены, то видимость работы тэна включена
	// а если оба тэна выключены, то видимость погашена
	AnimHeat34.setVisible(val);

	if ((Model::DFR_current.Ten1_Right ^ Model::DFR_current.Ten2_Right) == 1) {
		// Если включен только один тэн, то скорость волны маленькая
		AnimHeat34.setUpdateTicksInterval(8);
	}
	else {
		// Если включены два тэна, то скорость волны большая
		AnimHeat34.setUpdateTicksInterval(3);
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
