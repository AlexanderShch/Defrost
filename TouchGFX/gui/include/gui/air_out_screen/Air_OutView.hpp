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
    virtual void handleTickEvent();
protected:
    void syncExhaustFanAndFlapFromInputs();
};

#endif // AIR_OUTVIEW_HPP
