#ifndef SETTINGS4VIEW_HPP
#define SETTINGS4VIEW_HPP

#include <gui_generated/settings4_screen/Settings4ViewBase.hpp>
#include <gui/settings4_screen/Settings4Presenter.hpp>

class Settings4View : public Settings4ViewBase
{
public:
    Settings4View();
    virtual ~Settings4View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();    
    virtual void BTNManual3Clicked();
    virtual void BTNManual4Clicked();
    virtual void BTNFanControl3Clicked();
    virtual void BTNFanControl4Clicked();
protected:
};

#endif // SETTINGS4VIEW_HPP
