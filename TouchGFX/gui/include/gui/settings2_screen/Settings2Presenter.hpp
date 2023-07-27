#ifndef SETTINGS2PRESENTER_HPP
#define SETTINGS2PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Settings2View;

class Settings2Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Settings2Presenter(Settings2View& v);

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

    virtual ~Settings2Presenter() {};

private:
    Settings2Presenter();

    Settings2View& view;
};

#endif // SETTINGS2PRESENTER_HPP
