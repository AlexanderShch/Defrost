#include <gui/settings6_screen/Settings6View.hpp>
#include <gui/settings6_screen/Settings6Presenter.hpp>

Settings6Presenter::Settings6Presenter(Settings6View& v)
    : view(v)
{

}

void Settings6Presenter::activate()
{

}

void Settings6Presenter::deactivate()
{

}

void Settings6Presenter::Corr_Sensor_Addr(uint8_t SetAddress)
{
	Model::Index_CORR_sensor = SetAddress;
}

void Settings6Presenter::Corr_Scan(bool flag)
{
	if (flag)
		Model::Flag_CORR_ready = 1;
	else
		Model::Flag_CORR_ready = 0;
}
