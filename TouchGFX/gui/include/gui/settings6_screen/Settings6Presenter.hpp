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

    virtual void ValUpdatePresenter();										// функция запускается из Model, служит для обновления параметров на экране
    virtual void Corr_Sensor_Addr(uint8_t SetAddress);						// передача в обработку выбранного на экране адреса датчика для корректировки
    virtual void Corr_Sensor_Addr(uint8_t CORR_Type, int16_t CORR_Value); 	// передача значения корректировки в программу управления (Model)
    virtual void Corr_Scan(bool flag);										// установка флага разрешения сканирования датчика для корректировки
    virtual void Corr_Flag_Write(void);										// установка в программе управления (Model) флага разрешения записи в датчик

private:
    Settings6Presenter();

    Settings6View& view;
};

#endif // SETTINGS6PRESENTER_HPP
