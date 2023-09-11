#include <gui/visualization_screen/VisualizationView.hpp>
#include <touchgfx/utils.hpp>

//#include "string.h"
#include "stdio.h"

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
