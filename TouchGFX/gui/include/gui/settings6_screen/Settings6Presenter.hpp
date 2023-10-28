#ifndef SETTINGS6PRESENTER_HPP
#define SETTINGS6PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Settings6View;

class Settings6Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Settings6Presenter(Settings6View& v);

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

    virtual ~Settings6Presenter() {}

private:
    Settings6Presenter();

    Settings6View& view;
};

#endif // SETTINGS6PRESENTER_HPP
