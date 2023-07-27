#ifndef SETTINGS3VIEW_HPP
#define SETTINGS3VIEW_HPP

#include <gui_generated/settings3_screen/Settings3ViewBase.hpp>
#include <gui/settings3_screen/Settings3Presenter.hpp>

class Settings3View : public Settings3ViewBase
{
public:
    Settings3View();
    virtual ~Settings3View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void BTNManual1Clicked();
    virtual void BTNManual2Clicked();
    virtual void BTNFanControl1Clicked();
    virtual void BTNFanControl2Clicked();
protected:
};

#endif // SETTINGS3VIEW_HPP
