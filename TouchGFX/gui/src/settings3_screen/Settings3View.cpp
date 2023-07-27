#include <gui/settings3_screen/Settings3View.hpp>
#include <touchgfx/utils.hpp>
#include <iostream>

Settings3View::Settings3View()
{

}

void Settings3View::setupScreen()
{
    Settings3ViewBase::setupScreen();
}

void Settings3View::tearDownScreen()
{
    Settings3ViewBase::tearDownScreen();
}

void Settings3View::BTNManual1Clicked()
{
    if (BTNManualControl1.getState() == 0)
    {
        //When BTNManualControl1 clicked show BTNFanControl1
        //Show BTNFanControl1
        BTNFanControl1.setVisible(true);
        BTNFanControl1.invalidate();
    }
    else
    {
        //hide and disable fan and heat control
        BTNFanControl1.setVisible(false);
        BTNFanControl1.invalidate();
        BTNFanControl1.forceState(true);
        BTNHeatControl1.setVisible(false);
        BTNHeatControl1.forceState(true);
        BTNHeatControl1.invalidate();
    }
}

void Settings3View::BTNManual2Clicked()
{
    if (BTNManualControl2.getState() == 0)
    {
        //When BTNManualControl2 clicked show BTNFanControl2
        //Show BTNFanControl2
        BTNFanControl2.setVisible(true);
        BTNFanControl2.invalidate();
    }
    else
    {
        //hide and disable fan and heat control
        BTNFanControl2.setVisible(false);
        BTNFanControl2.invalidate();
        BTNFanControl2.forceState(true);
        BTNHeatControl2.setVisible(false);
        BTNHeatControl2.forceState(true);
        BTNHeatControl2.invalidate();
    }
}

void Settings3View::BTNFanControl1Clicked()
{
    if (BTNFanControl1.getState() == 0)
    {
        //When BTNFanControl1 clicked show BTNHeatControl1
        //Show BTNHeatControl1
        BTNHeatControl1.setVisible(true);
        BTNHeatControl1.invalidate();
    }
    else
    {
        //vice versa
        //turn HeatControl1 off
        BTNHeatControl1.setVisible(false);
        BTNHeatControl1.forceState(true);
        BTNHeatControl1.invalidate();
    }
}

void Settings3View::BTNFanControl2Clicked()
{
    if (BTNFanControl2.getState() == 0)
    {
        //When BTNFanControl2 clicked show BTNHeatControl2
        //Show BTNHeatControl2
        BTNHeatControl2.setVisible(true);
        BTNHeatControl2.invalidate();
    }
    else
    {
        //vice versa
        //turn HeatControl2 off
        BTNHeatControl2.setVisible(false);
        BTNHeatControl2.forceState(true);
        BTNHeatControl2.invalidate();
    }
    
}