#include <gui/settings2_screen/Settings2View.hpp>
#include <touchgfx/utils.hpp>
#include <iostream>

Settings2View::Settings2View()
{

}

void Settings2View::setupScreen()
{
    Settings2ViewBase::setupScreen();
}

void Settings2View::tearDownScreen()
{
    Settings2ViewBase::tearDownScreen();
}

void Settings2View::BTNGateManualClicked()
{
    if (BTNGateManual.getState() == 0)
    {
        //When BTNGateManual clicked show BTNGateControl
        //Show BTNGateControl
        BTNGateControl.setVisible(true);
        BTNGateControl.invalidate();
    }
    else
    {
        //hide button
        BTNGateControl.setVisible(false);
        BTNGateControl.invalidate();
    }
    
}

void Settings2View::BTNSprayManualClicked()
{
    if (BTNSprayManual.getState() == 0)
    {
        //When BTNSprayManual clicked show BTNSprayControl
        //Show BTNSprayControl
        BTNSprayControl.setVisible(true);
        BTNSprayControl.invalidate();
    }
    else
    {
        //vice versa
        //turn BTNSprayControl off
        BTNSprayControl.setVisible(false);
        BTNSprayControl.forceState(true);
        BTNSprayControl.invalidate();
    }
    
}