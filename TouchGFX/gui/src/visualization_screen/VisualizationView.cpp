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

// 0 - defroster left temperature
void VisualizationView::Val0UpdateView(float Val)
{
	Unicode::snprintfFloat(ValueDefrosterT1Buffer, sizeof(ValueDefrosterT1Buffer), "%.2f", Val);
	ValueDefrosterT1.invalidate();
}

// 1 - defroster right temperature
void VisualizationView::Val1UpdateView(float Val)
{
	Unicode::snprintfFloat(ValueDefrosterT2Buffer, sizeof(ValueDefrosterT2Buffer), "%.2f", Val);
	ValueDefrosterT2.invalidate();
}

// 2 - defroster center temperature
void VisualizationView::Val2UpdateView(float Val)
{
//	Unicode::snprintfFloat(ValueDefrosterT3Buffer, sizeof(ValueDefrosterT3Buffer), "%.2f", Val);
//	ValueDefrosterT3.invalidate();
}

// 3 - fish left temperature
void VisualizationView::Val3UpdateView(float Val)
{
	Unicode::snprintfFloat(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "%.2f", Val);
	ValueCoreT1Small.invalidate();
}

// 4 - fish right temperature
void VisualizationView::Val4UpdateView(float Val)
{
	Unicode::snprintfFloat(ValueCoreT2SmallBuffer, sizeof(ValueCoreT2SmallBuffer), "%.2f", Val);
	ValueCoreT2Small.invalidate();
}
