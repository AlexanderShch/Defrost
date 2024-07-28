#ifndef HOMEVIEW_HPP
#define HOMEVIEW_HPP

#include <gui_generated/home_screen/HomeViewBase.hpp>
#include <gui/home_screen/HomePresenter.hpp>

class HomeView : public HomeViewBase
{
public:
    HomeView();
    virtual ~HomeView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void Val_T_3UpdateView(int val);
    virtual void Val_T_4UpdateView(int val);
protected:
};

#endif // HOMEVIEW_HPP
