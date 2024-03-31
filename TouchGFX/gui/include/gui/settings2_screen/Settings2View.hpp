#ifndef SETTINGS2VIEW_HPP
#define SETTINGS2VIEW_HPP

#include <gui_generated/settings2_screen/Settings2ViewBase.hpp>
#include <gui/settings2_screen/Settings2Presenter.hpp>

class Settings2View : public Settings2ViewBase
{
public:
    Settings2View();
    virtual ~Settings2View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void BTNManualClicked();
    virtual void BTNSprayClicked();
    virtual void BTNGateUpClicked();
    virtual void BTNGateStopClicked();
    virtual void BTNGateDownClicked();

    virtual void SetAlpha(uint8_t MinAlhpa);

protected:
};

#endif // SETTINGS2VIEW_HPP
