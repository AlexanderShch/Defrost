#include <gui/settings5_screen/Settings5View.hpp>

/* Definitions for ProgrammingSens */
osThreadId_t ProgrammingSensHandle;
const osThreadAttr_t ProgrammingSens_attributes = {
  .name = "ProgrammingSens",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

void StartProgrammingSensor(void *argument);
extern void ProgrammingSensor(void);

Settings5View::Settings5View()
{

}

void Settings5View::setupScreen()
{
    Settings5ViewBase::setupScreen();
    /* creation of ProgrammingSens task */
    ProgrammingSensHandle = osThreadNew(StartProgrammingSensor, NULL, &ProgrammingSens_attributes);

}

void Settings5View::tearDownScreen()
{
    Settings5ViewBase::tearDownScreen();
    /* delete ProgrammingSens task */
    osThreadTerminate(ProgrammingSensHandle);
}

void StartProgrammingSensor(void *argument)
{
	ProgrammingSensor();
}

void Settings5View::scrollSensorSpeedNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex)
{
    switch (itemIndex)
    {
    case 0:
        item.updateScrollItem(2400);
        break;
    case 1:
    	item.updateScrollItem(4800);
        break;
    case 2:
    	item.updateScrollItem(9600);
        break;
    case 3:
    	item.updateScrollItem(19200);
        break;
    case 4:
    	item.updateScrollItem(38400);
        break;
    case 5:
    	item.updateScrollItem(115200);
        break;
    }
}
