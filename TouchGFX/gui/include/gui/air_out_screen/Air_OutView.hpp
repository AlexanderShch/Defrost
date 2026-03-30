#ifndef AIR_OUTVIEW_HPP
#define AIR_OUTVIEW_HPP

#include <gui_generated/air_out_screen/Air_OutViewBase.hpp>
#include <gui/air_out_screen/Air_OutPresenter.hpp>

class Air_OutView : public Air_OutViewBase
{
public:
    Air_OutView();
    virtual ~Air_OutView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void BTNManualClicked();
    virtual void BTN_AirFlapClicked();
    virtual void BTN_AirFanClicked();
    virtual void handleTickEvent();
protected:
    void syncExhaustFanAndFlapFromInputs();
    void updateAirOutManualControls();
};

#endif // AIR_OUTVIEW_HPP
