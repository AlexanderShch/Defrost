#include <gui/settings1_screen/Settings1View.hpp>
#include <touchgfx/utils.hpp>
#include <iostream>


Settings1View::Settings1View()
{

}

void Settings1View::setupScreen()
{
    //Updating the screen value to correspond with the CoreTSet Value
    Settings1ViewBase::setupScreen();
    Unicode::snprintf(ValueCoreTSetBuffer, VALUECORETSET_SIZE, "%d", CoreTSet);
    ValueCoreTSet.invalidate(); 
}

void Settings1View::tearDownScreen()
{
    Settings1ViewBase::tearDownScreen();
}

void Settings1View::BTNCoreTSetIncreaseClicked()
{
    // Checking the limit and increasing by 1 the CoreTSet and changing the screen value.
    if (CoreTSet <= 7)
    {
        CoreTSet++;
        Unicode::snprintf(ValueCoreTSetBuffer, VALUECORETSET_SIZE, "%d", CoreTSet);
        ValueCoreTSet.invalidate();
    }
}

void Settings1View::BTNCoreTSetDecreaseClicked()
{
    // Checking the limit and decreasing by 1 the CoreTSet and changing the screen value
    if (CoreTSet >= 1)
    {
        CoreTSet--;
        Unicode::snprintf(ValueCoreTSetBuffer, VALUECORETSET_SIZE, "%d", CoreTSet);
        ValueCoreTSet.invalidate();
    }
}
