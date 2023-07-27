#ifndef SETTINGS1PRESENTER_HPP
#define SETTINGS1PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Settings1View;

class Settings1Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Settings1Presenter(Settings1View& v);

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

    virtual ~Settings1Presenter() {};

private:
    Settings1Presenter();

    Settings1View& view;
};

#endif // SETTINGS1PRESENTER_HPP
