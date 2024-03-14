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
// обновим данные на экране
void Settings6Presenter::ValUpdatePresenter()
{
	if (Model::Flag_Corr_T_changed == 1)
		view.CorrData_T_View();
	switch (Model::Type_CORR_sensor) {
		case 1:		{
			if (Model::Flag_Corr_H_changed == 1)
				view.CorrData_H_View();
			break;	}
		case 2:		{
			if (Model::Flag_Corr_R_changed == 1)
				view.CorrData_R_View();
			break;	}
		default:
			break;

	}
}

// передадим адрес в программу управления (Model)
void Settings6Presenter::Corr_Sensor_Address(uint8_t SetAddress)
{
	Model::Index_CORR_sensor = SetAddress;
}

// передадим значения корректировки в программу управления (Model)
void Settings6Presenter::Corr_Sensor_Value(uint8_t  CORR_Type, int16_t CORR_Value)
{
	switch (CORR_Type) {
		case 2:	// T
			Model::CORR_T_sensor = CORR_Value;
			break;
		case 3:	// H
			Model::CORR_H_sensor = CORR_Value;
			break;
		case 4:	// R
			Model::CORR_R_sensor = CORR_Value;
			break;
		default:
			break;
	}
}

// установим в программе управления (Model) флаг разрешения записи в датчик
void Settings6Presenter::Corr_Flag_Write()
{
			Model::Flag_WR_to_sensor = 1;
}
// сканирование датчика разрешено/запрещено программирование и установка флага в Model
void Settings6Presenter::Corr_Scan(bool flag)
{
	if (flag)
		Model::Flag_CORR_ready = 1;
	else
		Model::Flag_CORR_ready = 0;
}

// установим в программе управления (Model) флаг обнуления корректировки датчика типа 1
void Settings6Presenter::Reset_Flag_Write()
{
			Model::Flag_Alert = 1;
			Model::T_CORR_sensor = 0;
			Model::H_CORR_sensor = 0;
}
