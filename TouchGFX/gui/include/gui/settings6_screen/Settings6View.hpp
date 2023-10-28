#ifndef SETTINGS6VIEW_HPP
#define SETTINGS6VIEW_HPP

#include <gui_generated/settings6_screen/Settings6ViewBase.hpp>
#include <gui/settings6_screen/Settings6Presenter.hpp>

class Settings6View : public Settings6ViewBase
{
public:
    Settings6View();
    virtual ~Settings6View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void scrollWheelDigit1UpdateItem(DigitContainer& item, int16_t itemIndex);
    virtual void scrollWheelDigit1UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex);
    virtual void scrollWheelDigit2UpdateItem(DigitContainer& item, int16_t itemIndex);
    virtual void scrollWheelDigit2UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex);
    virtual void scrollWheelDigit3UpdateItem(DigitContainer& item, int16_t itemIndex);
    virtual void scrollWheelDigit3UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex);
    virtual void scrollWheelDigit4UpdateItem(DigitContainer& item, int16_t itemIndex);
    virtual void scrollWheelDigit4UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex);
    virtual void scrollSensorTypeUpdateItem(TextContainer& item, int16_t itemIndex);
    virtual void scrollSensorTypeUpdateCenterItem(TextSelectedContainer& item, int16_t itemIndex);
    virtual void scrollSensorAddressUpdateItem(ScrollItemContainer& item, int16_t itemIndex);
    virtual void scrollSensorAddressUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex);

   /*
     * Virtual Action Handlers
     */
    virtual void BTNWriteClicked();
    virtual void BTNConfirmClicked();
    virtual void BTNCancelClicked();
    virtual void BTNSensorTypeClicked();
    virtual void BTNSetAddressClicked();

    virtual void Print_Whole_Digit(void);

    int16_t Set_Digit;
    int8_t Digit1, Digit2, Digit3, Digit4;
    uint8_t Selected;				// нажатая кнопка: 0 - тип, 1 - адрес
    int16_t SetSensor = 0;			// вновь устанавливаемое значение типа датчика, выводится на экран
    int16_t SetSensorOld = 0;		// старое значение типа датчика, сохраняется на время выбора в колесе нового значения
    uint8_t SetAddress;				// вновь установленное значение адреса
    uint8_t SetAddressOld;			// старое значение адреса датчика, сохраняется на время выбора в колесе нового значения
    uint8_t FlagWrite_visible = 0;	// флаг разрешения видимости кнопки "Запись" в датчик


protected:
    // В классе Settings6View
    // описываются смарт-пойнтеры класса Callback на функции, обслуживающие обратный вызов
    // в скобках <...> определён шаблон для передачи в класс Callback - тип вызывающего функцию объекта и параметр для вызова
    Callback<Settings6View, int16_t> scrollDigit1ItemSelectedCallback;
    void scrollDigit1ItemSelectedHandler(int16_t itemSelected);
    Callback<Settings6View, int16_t> scrollDigit2ItemSelectedCallback;
    void scrollDigit2ItemSelectedHandler(int16_t itemSelected);
    Callback<Settings6View, int16_t> scrollDigit3ItemSelectedCallback;
    void scrollDigit3ItemSelectedHandler(int16_t itemSelected);
    Callback<Settings6View, int16_t> scrollDigit4ItemSelectedCallback;
    void scrollDigit4ItemSelectedHandler(int16_t itemSelected);
    Callback<Settings6View, int16_t> scrollSensorTypeItemSelectedCallback;
    void scrollSensorTypeItemSelectedHandler(int16_t itemSelected);
    Callback<Settings6View, int16_t> scrollSensorAddressItemSelectedCallback;
    void scrollSensorAddressItemSelectedHandler(int16_t itemSelected);

};

#endif // SETTINGS6VIEW_HPP
