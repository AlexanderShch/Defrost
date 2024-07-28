#include <gui/home_screen/HomeView.hpp>

HomeView::HomeView()
{

}

void HomeView::setupScreen()
{
    HomeViewBase::setupScreen();
}

void HomeView::tearDownScreen()
{
    HomeViewBase::tearDownScreen();
}

// 3 - fish left temperature
void HomeView::Val_T_3UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT1Buffer, sizeof(ValueCoreT1Buffer), "%.1f", (float)Val/10);
	ValueCoreT1.invalidate();
}

// 4 - fish right temperature
void HomeView::Val_T_4UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT2Buffer, sizeof(ValueCoreT2Buffer), "%.1f", (float)Val/10);
	ValueCoreT2.invalidate();
}
