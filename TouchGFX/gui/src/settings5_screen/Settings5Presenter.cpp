#include <gui/settings5_screen/Settings5View.hpp>
#include <gui/settings5_screen/Settings5Presenter.hpp>

extern int BaudRate[];

Settings5Presenter::Settings5Presenter(Settings5View& v)
    : view(v)
{

}

void Settings5Presenter::activate()
{

}

void Settings5Presenter::deactivate()
{

}

void Settings5Presenter::ValUpdatePresenter()
{
	if (Model::getFlagCurrentVal_PR_Chng() == 1)
	{
		Model::clearFlagCurrentVal_PR_Chng();
		view.Val_Addr_UpdateView(Model::getCurrentAddress_PR());
		view.Val_BaudRate_UpdateView(Model::getCurrentBaudRate_PR());
	}
}


void Settings5Presenter::PR_Sensor_Data_Write(uint8_t SetSpeed, uint8_t SetAddress)
{
	Model::BaudRate_WR_to_sensor = SetSpeed;
	Model::Address_WR_to_sensor = SetAddress;
	Model::Flag_WR_to_sensor = 1;
}

void Settings5Presenter::PR_Sensor_Type(uint8_t SetSensor)
{
	Model::Type_of_sensor = SetSensor;
}

