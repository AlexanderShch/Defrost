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

// Эта процедура вызывается из Model для вывода на экран
void Settings5Presenter::ValUpdatePresenter()
{
	if (Model::Flag_Alert == 1) {
		view.Alert_Message_Output();
	}

	if (Model::getFlagCurrentVal_PR_Chng() == 1)
	{
		Model::clearFlagCurrentVal_PR_Chng();
		view.Val_Addr_UpdateView(Model::getCurrentAddress_PR());
		view.Val_BaudRate_UpdateView(Model::getCurrentBaudRate_PR());
	}
}

// здесь передаем в Model установленные параметры скорости и адреса для программирования в датчик
void Settings5Presenter::PR_Sensor_Data_Write(uint8_t SetSpeed, uint8_t SetAddress)
{
	Model::BaudRate_WR_to_sensor = SetSpeed;
	Model::Address_WR_to_sensor = SetAddress;
	Model::Flag_WR_to_sensor = 1;
}

// здесь передаём в Model установленный тип датчика для программирования
void Settings5Presenter::PR_Sensor_Type(uint8_t SetSensor)
{
	Model::Type_of_sensor = SetSensor;
}

