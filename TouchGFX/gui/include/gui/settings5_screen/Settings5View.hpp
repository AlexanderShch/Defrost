#ifndef SETTINGS5VIEW_HPP
#define SETTINGS5VIEW_HPP

#include <gui_generated/settings5_screen/Settings5ViewBase.hpp>
#include <gui/settings5_screen/Settings5Presenter.hpp>

#include "cmsis_os2.h"                  // ::CMSIS:RTOS2
#include "ModBus.hpp"

class Settings5View : public Settings5ViewBase
{
public:
    Settings5View();
    virtual ~Settings5View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void scrollSensorSpeedNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex);
    virtual void scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex);
    virtual void BTNWriteClicked();
    virtual void BTNSetSpeedClicked();
    virtual void BTNConfirmClicked();
    virtual void BTNCancelClicked();
protected:

};

#endif // SETTINGS5VIEW_HPP


