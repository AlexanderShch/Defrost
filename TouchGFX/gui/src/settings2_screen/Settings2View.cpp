#include <gui/settings2_screen/Settings2View.hpp>
#include <gui/model/Model.hpp>

#include <touchgfx/utils.hpp>
#include <iostream>
uint8_t MinAlpha = 50;
uint16_t *pDFR_manual = (uint16_t*) &Model::DFR_manual;	// указатель на регистр DFR_manual


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

void Settings2View::BTNManualClicked()
{
   if (BTN_Manual.getState() == 1)
    {
	   // Ручной режим ВКЛ, автоматический режим ВЫКЛ
	   Model::Flag_DFR_manual = 1;
       //When BTNManual clicked show BTNGateControl
       //Show BTNGateControl
	   Settings2View::SetAlpha(255);
    }
    else
    {
    	// Автоматический режим ВКЛ, ручной режим ВЫКЛ
 	    Model::Flag_DFR_manual = 0;
 	    *pDFR_manual = 0;
 	    //hide button
    	Settings2View::SetAlpha(MinAlpha);
    }
}

void Settings2View::SetAlpha(uint8_t MinAlhpa)
{
   	BTN_GateUP.setAlpha(MinAlpha);
   	BTN_GateDOWN.setAlpha(MinAlpha);
   	BTN_GateSTOP.setAlpha(MinAlpha);
	BTN_Spray.setAlpha(MinAlpha);
	LabelSprayOn.setAlpha(MinAlpha);
	LabelSprayOff.setAlpha(MinAlpha);
	BTN_GateUP.invalidate();
	BTN_GateDOWN.invalidate();
	BTN_GateSTOP.invalidate();
	BTN_Spray.invalidate();
	LabelSprayOn.invalidate();
	LabelSprayOff.invalidate();
}

void Settings2View::BTNSprayClicked()
{
    if (BTN_Spray.getState() == 1)
    {
    	// Включим бит управления водным клапаном
    	Model::DFR_manual.Water_Flap = 1;
    }else{
    	Model::DFR_manual.Water_Flap = 0;
     }
}
void Settings2View::BTNGateUpClicked()
{
    if (BTN_GateUP.getPressedState() == 1)
    {
    	// Включим бит управления поднять ворота
    	Model::DFR_manual.Gate_Up = 1;
    }else{
    	// Включим бит управления опустить ворота
    	Model::DFR_manual.Gate_Up = 0;
     }
}
void Settings2View::BTNGateStopClicked()
{
    if (BTN_GateSTOP.getPressedState() == 1)
    {
    	// Включим бит управления поднять ворота
    	Model::DFR_manual.Gate_Stop = 1;
    }else{
    	// Включим бит управления опустить ворота
    	Model::DFR_manual.Gate_Stop = 0;
     }
}
void Settings2View::BTNGateDownClicked()
{
    if (BTN_GateDOWN.getPressedState() == 1)
    {
    	// Включим бит управления поднять ворота
    	Model::DFR_manual.Gate_Down = 1;
    }else{
    	// Включим бит управления опустить ворота
    	Model::DFR_manual.Gate_Down = 0;
     }
}
