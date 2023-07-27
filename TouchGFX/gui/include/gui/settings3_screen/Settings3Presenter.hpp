#ifndef SETTINGS3PRESENTER_HPP
#define SETTINGS3PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Settings3View;

class Settings3Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Settings3Presenter(Settings3View& v);

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

    virtual ~Settings3Presenter() {};

private:
    Settings3Presenter();

    Settings3View& view;
};

#endif // SETTINGS3PRESENTER_HPP
