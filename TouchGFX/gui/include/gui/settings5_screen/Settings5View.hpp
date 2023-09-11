#ifndef SETTINGS5VIEW_HPP
#define SETTINGS5VIEW_HPP

#include <gui_generated/settings5_screen/Settings5ViewBase.hpp>
#include <gui/settings5_screen/Settings5Presenter.hpp>

class Settings5View : public Settings5ViewBase
{
public:
    Settings5View();
    virtual ~Settings5View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // SETTINGS5VIEW_HPP
