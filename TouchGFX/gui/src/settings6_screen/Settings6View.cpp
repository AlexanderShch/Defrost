#include <gui/settings6_screen/Settings6View.hpp>
#include <gui_generated/containers/TextContainerBase.hpp>

#include "string.h"

extern SENSOR_Type_t Sensor_type[STQ];
extern SENSOR_typedef_t Sensor_array[SQ];


Settings6View::Settings6View():
	//Создание смарт-пойнтеров на функции Handler для Callback
	// в функция callback будет передана ссылка на установленную функцию обработки
	scrollDigit1ItemSelectedCallback(this, &Settings6View::scrollDigit1ItemSelectedHandler),
	scrollDigit2ItemSelectedCallback(this, &Settings6View::scrollDigit2ItemSelectedHandler),
	scrollDigit3ItemSelectedCallback(this, &Settings6View::scrollDigit3ItemSelectedHandler),
	scrollDigit4ItemSelectedCallback(this, &Settings6View::scrollDigit4ItemSelectedHandler),
	scrollSensorTypeItemSelectedCallback(this, &Settings6View::scrollSensorTypeItemSelectedHandler),
	scrollSensorAddressItemSelectedCallback(this, &Settings6View::scrollSensorAddressItemSelectedHandler)
{

}

void Settings6View::setupScreen()
{
    Settings6ViewBase::setupScreen();
    Set_Digit = 0;
    Digit1 = 0;
    Digit2 = 0;
    Digit3 = 0;
    Digit4 = 0;
    //В контейнерах колёс прокрутки устанавливаются Callback функции обработки события
    //например, для передачи значения ItemSelected при окончании анимации скролла при выборе значения в колесе
    scrollWheelDigit1.setItemSelectedCallback(scrollDigit1ItemSelectedCallback);
    scrollWheelDigit2.setItemSelectedCallback(scrollDigit2ItemSelectedCallback);
    scrollWheelDigit3.setItemSelectedCallback(scrollDigit3ItemSelectedCallback);
    scrollWheelDigit4.setItemSelectedCallback(scrollDigit4ItemSelectedCallback);
    scrollSensorType.setItemSelectedCallback(scrollSensorTypeItemSelectedCallback);
    scrollSensorAddress.setItemSelectedCallback(scrollSensorAddressItemSelectedCallback);

    FlagWrite_visible = 0;
//    SetSpeed = 0;
    SetAddress = 0;
	SetSensor = 0;
//	presenter -> PR_Sensor_Type(SetSensor);
}

void Settings6View::tearDownScreen()
{
    Settings6ViewBase::tearDownScreen();
}

/************* DIGIT1 *********************/
void Settings6View::scrollWheelDigit1UpdateItem(DigitContainer& item, int16_t itemIndex)
{
	item.updateDigitItem(itemIndex);
}
void Settings6View::scrollWheelDigit1UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex)
{
	item.updateDigitSelectedItem(itemIndex);
}
/************* DIGIT2 *********************/
void Settings6View::scrollWheelDigit2UpdateItem(DigitContainer& item, int16_t itemIndex)
{
	item.updateDigitItem(itemIndex);
}
void Settings6View::scrollWheelDigit2UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex)
{
	item.updateDigitSelectedItem(itemIndex);
}
/************* DIGIT3 *********************/
void Settings6View::scrollWheelDigit3UpdateItem(DigitContainer& item, int16_t itemIndex)
{
	item.updateDigitItem(itemIndex);
}
void Settings6View::scrollWheelDigit3UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex)
{
	item.updateDigitSelectedItem(itemIndex);
}
/************* DIGIT4 *********************/
void Settings6View::scrollWheelDigit4UpdateItem(DigitContainer& item, int16_t itemIndex)
{
	item.updateDigitItem(itemIndex);
}
void Settings6View::scrollWheelDigit4UpdateCenterItem(DigitSelectedContainer& item, int16_t itemIndex)
{
	item.updateDigitSelectedItem(itemIndex);
}
/************** TYPE **********************/
void Settings6View::scrollSensorTypeUpdateItem(TextContainer& item, int16_t itemIndex)
{
//	Формирование строки с именем типа датчика и его идентификатором
	char str[20];
	char FormatText[] = " %d";
	strcpy(str, Sensor_type[itemIndex].TypeName);		// копирование PositionName в начало строки str, включая символ окончания строки \0
	strcat(str, FormatText);							// добавление символа форматирования в строку str
	item.updateTextItem(str, Sensor_type[itemIndex].TypeNumber);
}
void Settings6View::scrollSensorTypeUpdateCenterItem(TextSelectedContainer& item, int16_t itemIndex)
{
	//	Формирование строки с именем типа датчика и его идентификатором
		char str[20];
		char FormatText[] = " %d";
		strcpy(str, Sensor_type[itemIndex].TypeName);		// копирование PositionName в начало строки str, включая символ окончания строки \0
		strcat(str, FormatText);							// добавление символа форматирования в строку str
		item.updateTextSelectedItem(str, Sensor_type[itemIndex].TypeNumber);
}
/************** ADDRESS ******************/
void Settings6View::scrollSensorAddressUpdateItem(TextContainer& item, int16_t itemIndex)
{
	//	Формирование строки с именем датчика и его адресом
		char str[20];
		char FormatText[] = " %d";
		strcpy(str, Sensor_array[itemIndex].PositionName);	// копирование PositionName в начало строки str, включая символ окончания строки \0
		strcat(str, FormatText);							// добавление символа форматирования в строку str
		item.updateTextItem(str, Sensor_array[itemIndex].Address);
}
void Settings6View::scrollSensorAddressUpdateCenterItem(TextSelectedContainer& item, int16_t itemIndex)
{
	//	Формирование строки с именем типа датчика и его идентификатором
		char str[20];
		char FormatText[] = " %d";
		strcpy(str, Sensor_array[itemIndex].PositionName);	// копирование PositionName в начало строки str, включая символ окончания строки \0
		strcat(str, FormatText);							// добавление символа форматирования в строку str
		item.updateTextSelectedItem(str, Sensor_array[itemIndex].Address);
}
/************* HANDLERS ******************/
/*
 * Virtual Action Handlers
 */
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings6View::scrollDigit1ItemSelectedHandler(int16_t itemSelected)
{
	Digit1 = itemSelected;
	Print_Whole_Digit();
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings6View::scrollDigit2ItemSelectedHandler(int16_t itemSelected)
{
	Digit2 = itemSelected;
	Print_Whole_Digit();
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings6View::scrollDigit3ItemSelectedHandler(int16_t itemSelected)
{
	Digit3 = itemSelected;
	Print_Whole_Digit();
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings6View::scrollDigit4ItemSelectedHandler(int16_t itemSelected)
{
	Digit4 = itemSelected;
	Print_Whole_Digit();
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings6View::scrollSensorTypeItemSelectedHandler(int16_t itemSelected)
{
	SetSensor = itemSelected;
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings6View::scrollSensorAddressItemSelectedHandler(int16_t itemSelected)
{
	SetAddress = Sensor_array[itemSelected].Address;
}
/******************************************/
/*********** BUTTONS **********************/
void Settings6View::BTNWriteClicked()
{
    // Override and implement this function in Settings6
}
void Settings6View::BTNConfirmClicked()
{
	switch (Selected)
	{
		case 0:		{
			SetSensorOld = SetSensor;
			Unicode::snprintf(BTNSensorTypeBuffer, BTNSENSORTYPE_SIZE, "%d", SetSensor);
			scrollSensorType.setVisible(false);
			scrollSensorType.invalidate();
//			presenter -> PR_Sensor_Type(SetSensor);
			break;	}
		case 1:		{
			SetAddressOld = SetAddress;
			Unicode::snprintf(BTNSetAddressBuffer, BTNSETADDRESS_SIZE, "%d", SetAddress);
//			BTNSetAddress.invalidate();
			scrollSensorAddress.setVisible(false);
			scrollSensorAddress.invalidate();
			break;	}
		case 2:
//			SetSpeedOld = SetSpeed;
//			switch (SetSensor) {
//				case 1:		{
//					Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", BaudRate_Type1[SetSpeed]);
//					break;	}
//				case 2:		{
//					Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", BaudRate_Type2[SetSpeed]);
//					break;	}
//				default:
//					break;
//			}
//			scrollSensorSpeedNew.setVisible(false);
			break;
	}
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
//	if ((FlagWrite_visible == 1)&&(SetSpeed != 0)&&(SetAddress != 0))
//		BTNWrite.setVisible(true);
//	else
//		BTNWrite.setVisible(false);
	BTNWrite.invalidate();
	Settings6ViewBase::setupScreen();
}
void Settings6View::BTNCancelClicked()
{
	switch (Selected)
	{
		case 0:		{
			SetSensor = SetSensorOld;
			scrollSensorType.setVisible(false);
			scrollSensorType.invalidate();
			break;	}
		case 1:		{
			SetAddress = SetAddressOld;
			scrollSensorAddress.setVisible(false);
			scrollSensorAddress.invalidate();
			break;	}
		case 2:
//			SetSpeed = SetSpeedOld;
//			scrollSensorSpeedNew.setVisible(false);
			break;
		BTNConfirm.setVisible(false);
		BTNCancel.setVisible(false);
		BTNConfirm.invalidate();
//		BTNCancel.invalidate();
//		if ((FlagWrite_visible == 1)&&(SetSpeed != 0)&&(SetAddress != 0))
//			BTNWrite.setVisible(true);
//		else
//			BTNWrite.setVisible(false);
		BTNWrite.invalidate();
		Settings6ViewBase::setupScreen();
	}
}
//Запуск колеса прокрутки "Выбор типа датчика"
void Settings6View::BTNSensorTypeClicked()
{
	Selected = 0;
	SetSensorOld = SetSensor;
	scrollSensorType.setVisible(true);
	scrollSensorType.invalidate();
	BTNConfirm.setVisible(true);
	BTNConfirm.invalidate();
	BTNCancel.setVisible(true);
	BTNCancel.invalidate();
	BTNWrite.setVisible(false);
	BTNWrite.invalidate();
}
//Запуск колеса прокрутки "Выбор адреса датчика"
void Settings6View::BTNSetAddressClicked()
{
	Selected = 1;
	SetAddressOld = SetAddress;
	scrollSensorAddress.setVisible(true);
	scrollSensorAddress.invalidate();
	BTNConfirm.setVisible(true);
	BTNConfirm.invalidate();
	BTNCancel.setVisible(true);
	BTNCancel.invalidate();
	BTNWrite.setVisible(false);
	BTNWrite.invalidate();
}
/************************************************/

// Вывод на печать подобранного колёсами прокрутки значения числа
void Settings6View::Print_Whole_Digit(void)
{
	Set_Digit = Digit1 + Digit2*10 + Digit3*100 + Digit4*1000;
	Unicode::snprintfFloat(Whole_DigitBuffer, sizeof(Whole_DigitBuffer), "%05.1f", (float)Set_Digit/10);
	Whole_Digit.invalidate();
}

