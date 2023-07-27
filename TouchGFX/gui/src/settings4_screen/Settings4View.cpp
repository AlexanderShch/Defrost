#include <gui/settings4_screen/Settings4View.hpp>
#include <touchgfx/utils.hpp>
#include <iostream>

Settings4View::Settings4View()
{

}

void Settings4View::setupScreen()
{
    Settings4ViewBase::setupScreen();
}

void Settings4View::tearDownScreen()
{
    Settings4ViewBase::tearDownScreen();
}

void Settings4View::BTNManual3Clicked()
{
    if (BTNManualControl3.getState() == 0)
    {
        //When BTNManualControl3 clicked show BTNFanControl1
        //Show BTNFanControl3
        BTNFanControl3.setVisible(true);
        BTNFanControl3.invalidate();
    }
    else
    {
        //hide and disable fan and heat control
        BTNFanControl3.setVisible(false);
        BTNFanControl3.invalidate();
        BTNFanControl3.forceState(true);
        BTNHeatControl3.setVisible(false);
        BTNHeatControl3.forceState(true);
        BTNHeatControl3.invalidate();
    }
}

void Settings4View::BTNManual4Clicked()
{
    if (BTNManualControl4.getState() == 0)
    {
        //When BTNManualControl4 clicked show BTNFanControl2
        //Show BTNFanControl4
        BTNFanControl4.setVisible(true);
        BTNFanControl4.invalidate();
    }
    else
    {
        //hide and disable fan and heat control
        BTNFanControl4.setVisible(false);
        BTNFanControl4.invalidate();
        BTNFanControl4.forceState(true);
        BTNHeatControl4.setVisible(false);
        BTNHeatControl4.forceState(true);
        BTNHeatControl4.invalidate();
    }
}

void Settings4View::BTNFanControl3Clicked()
{
    if (BTNFanControl3.getState() == 0)
    {
        //When BTNFanControl3 clicked show BTNHeatControl1
        //Show BTNHeatControl3
        BTNHeatControl3.setVisible(true);
        BTNHeatControl3.invalidate();
    }
    else
    {
        //vice versa
        //turn HeatControl3 off
        BTNHeatControl3.setVisible(false);
        BTNHeatControl3.forceState(true);
        BTNHeatControl3.invalidate();
    }
}

void Settings4View::BTNFanControl4Clicked()
{
    if (BTNFanControl4.getState() == 0)
    {
        //When BTNFanControl4 clicked show BTNHeatControl2
        //Show BTNHeatControl4
        BTNHeatControl4.setVisible(true);
        BTNHeatControl4.invalidate();
    }
    else
    {
        //vice versa
        //turn HeatControl4 off
        BTNHeatControl4.setVisible(false);
        BTNHeatControl4.forceState(true);
        BTNHeatControl4.invalidate();
    }
    
}