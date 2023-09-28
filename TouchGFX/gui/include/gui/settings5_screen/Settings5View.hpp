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

    virtual void scrollSensorTypeUpdateItem(ScrollItemContainer& item, int16_t itemIndex);
    virtual void scrollSensorTypeUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex);
    virtual void scrollSensorSpeedNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex);
    virtual void scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex);
    virtual void scrollSensorAddressNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex);
    virtual void scrollSensorAddressNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex);
    virtual void BTNWriteClicked();
    virtual void BTNSensorTypeClicked();
    virtual void BTNSetSpeedClicked();
    virtual void BTNSetAddressClicked();
    virtual void BTNConfirmClicked();
    virtual void BTNCancelClicked();

    virtual void Val_Addr_UpdateView(uint8_t Val);
    virtual void Val_BaudRate_UpdateView(uint8_t Val);
protected:
    Callback<Settings5View, int16_t> scrollSensorTypeItemSelectedCallback;
    void scrollSensorTypeItemSelectedHandler(int16_t itemSelected);
    Callback<Settings5View, int16_t> scrollSensorSpeedNewItemSelectedCallback;
    void scrollSensorSpeedNewItemSelectedHandler(int16_t itemSelected);
    Callback<Settings5View, int16_t> scrollSensorAddressNewItemSelectedCallback;
    void scrollSensorAddressNewItemSelectedHandler(int16_t itemSelected);

};

#endif // SETTINGS5VIEW_HPP


