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
void VisualizationView::Val0UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterT1Buffer, sizeof(ValueDefrosterT1Buffer), "%.1f", Val/10);
	ValueDefrosterT1.invalidate();
}

// 1 - defroster right temperature
void VisualizationView::Val1UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueDefrosterT2Buffer, sizeof(ValueDefrosterT2Buffer), "%.1f", Val/10);
	ValueDefrosterT2.invalidate();
}

// 2 - defroster center temperature
void VisualizationView::Val2UpdateView(int Val)
{
//	Unicode::snprintfFloat(ValueDefrosterT3Buffer, sizeof(ValueDefrosterT3Buffer), "%.1f", Val/10);
//	ValueDefrosterT3.invalidate();
}

// 3 - fish left temperature
void VisualizationView::Val3UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT1SmallBuffer, sizeof(ValueCoreT1SmallBuffer), "%.1f", Val/10);
	ValueCoreT1Small.invalidate();
}

// 4 - fish right temperature
void VisualizationView::Val4UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT2SmallBuffer, sizeof(ValueCoreT2SmallBuffer), "%.1f", Val/10);
	ValueCoreT2Small.invalidate();
}
