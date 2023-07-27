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
    virtual void BTNGateManualClicked();
    virtual void BTNSprayManualClicked();
protected:
};

#endif // SETTINGS2VIEW_HPP
