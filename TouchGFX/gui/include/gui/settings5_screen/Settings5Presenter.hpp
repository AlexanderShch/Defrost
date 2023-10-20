#ifndef SETTINGS5PRESENTER_HPP
#define SETTINGS5PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Settings5View;

class Settings5Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    Settings5Presenter(Settings5View& v);

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

    virtual ~Settings5Presenter() {}

    virtual void ValUpdatePresenter();
    virtual void PR_Sensor_Data_Write(uint8_t SetSpeed, uint8_t SetAddress);
    virtual void PR_Sensor_Type(uint8_t SetSensor);
private:
    Settings5Presenter();

    Settings5View& view;
};

#endif // SETTINGS5PRESENTER_HPP
