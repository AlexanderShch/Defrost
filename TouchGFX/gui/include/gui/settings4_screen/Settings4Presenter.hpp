#ifndef SETTINGS4PRESENTER_HPP
#define SETTINGS4PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Settings4View;

class Settings4Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Settings4Presenter(Settings4View& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~Settings4Presenter() {};

private:
    Settings4Presenter();

    Settings4View& view;
};

#endif // SETTINGS4PRESENTER_HPP
