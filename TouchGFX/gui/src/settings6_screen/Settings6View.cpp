#include <gui/settings6_screen/Settings6View.hpp>
#include <gui_generated/containers/TextContainerBase.hpp>

#include "string.h"

extern SENSOR_Type_t Sensor_type[STQ];
extern SENSOR_typedef_t Sensor_array[SQ];

Settings6View::Settings6View():
	//Создание смарт-пойнтеров на функции Handler для Callback
	// в функцию callback будет передана ссылка на установленную функцию обработки
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
	Unicode::strncpy(BTN_T_CorrectionBuffer, "---", 4); //buffer belongs to textArea
	Unicode::strncpy(BTN_H_CorrectionBuffer, "---", 4); //buffer belongs to textArea
	Unicode::strncpy(BTN_R_CorrectionBuffer, "---", 4); //buffer belongs to textArea
	FlagWriteT = 0;
	FlagWriteH = 0;
	FlagWriteR = 0;
	Model::T_CORR_sensor = 0;
	Model::H_CORR_sensor = 0;
	Model::R_CORR_sensor = 0;
	Model::Type_CORR_sensor = 0;
	Settings6View::CorrData_T_View();
	Settings6View::CorrData_H_View();
	Settings6View::CorrData_R_View();

	//В контейнерах колёс прокрутки устанавливаются Callback функции обработки события
    //например, для передачи значения ItemSelected при окончании анимации скролла при выборе значения в колесе
    scrollWheelDigit1.setItemSelectedCallback(scrollDigit1ItemSelectedCallback);
    scrollWheelDigit2.setItemSelectedCallback(scrollDigit2ItemSelectedCallback);
    scrollWheelDigit3.setItemSelectedCallback(scrollDigit3ItemSelectedCallback);
    scrollWheelDigit4.setItemSelectedCallback(scrollDigit4ItemSelectedCallback);
    scrollSensorType.setItemSelectedCallback(scrollSensorTypeItemSelectedCallback);
    scrollSensorAddress.setItemSelectedCallback(scrollSensorAddressItemSelectedCallback);

    SetAddress = 0;
	SetSensor = 0;
	presenter -> Corr_Scan(false);	// датчик не выбран, сканирование датчика не разрешено
}

void Settings6View::tearDownScreen()
{
    Settings6ViewBase::tearDownScreen();
    SetAddress = 0;
	SetSensor = 0;
	presenter -> Corr_Scan(false);	// датчик не выбран, сканирование датчика не разрешено
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
	SetAddress = itemSelected;
}
/******************************************/
/*********** BUTTONS **********************/
void Settings6View::BTNWriteClicked()
{
	presenter -> Corr_Flag_Write();
}

void Settings6View::BTNResetClicked()
{
	presenter -> Reset_Flag_Write();
	// установку корректировки обнулим на экране и передадим в Model
	Set_Digit = 0;
	Selected = 2;
	Unicode::strncpy(BTN_T_CorrectionBuffer, "---", 4); //buffer belongs to textArea
	presenter -> Corr_Sensor_Value(Selected, Set_Digit);
	Selected = 3;
	Unicode::strncpy(BTN_H_CorrectionBuffer, "---", 4); //buffer belongs to textArea
	presenter -> Corr_Sensor_Value(Selected, Set_Digit);
	DigitWheelOff();
	CorrData_T_View();
	CorrData_H_View();
}

void Settings6View::BTNConfirmClicked()
{
	switch (Selected)
	{
		case 1:		{	// выбор адреса датчика
			SetAddressOld = SetAddress;
			Unicode::snprintf(BTNSetAddressBuffer, BTNSETADDRESS_SIZE, "%d", Sensor_array[SetAddress].Address);
			scrollSensorAddress.setVisible(false);
			scrollSensorAddress.invalidate();
			// передадим адрес в программу управления
			presenter -> Corr_Sensor_Address(SetAddress);
			presenter -> Corr_Scan(true);	// датчик выбран, сканирование датчика разрешено
			Model::Type_CORR_sensor = Sensor_array[SetAddress].TypeOfSensor;
			Unicode::snprintf(BTNSensorTypeBuffer, BTNSENSORTYPE_SIZE, "%d", Model::Type_CORR_sensor);

			// включим видимость кнопки Reset для датчика типа 1
			if (Model::Type_CORR_sensor == 1)
				BTNReset.setVisible(true);
			else
				BTNReset.setVisible(false);
			BTNReset.invalidate();

			DigitWheelOff();			// тип окна корректировки - 0, разрешено отображение Н и R
			break;	}
		case 2:		{	// установка корректировочного значения T
			if (Set_Digit != 0)
				Unicode::snprintfFloat(BTN_T_CorrectionBuffer, BTN_T_CORRECTION_SIZE, "%5.1f", (float)Set_Digit/10);
			else
				Unicode::strncpy(BTN_T_CorrectionBuffer, "---", 4); //buffer belongs to textArea
			presenter -> Corr_Sensor_Value(Selected, Set_Digit);
			if (Set_Digit == 0) FlagWriteT = 0; else FlagWriteT = 1;
			DigitWheelOff();
			break;	}
		case 3:		{	// установка корректировочного значения H
			if (Set_Digit != 0)
				Unicode::snprintfFloat(BTN_H_CorrectionBuffer, BTN_H_CORRECTION_SIZE, "%5.1f", (float)Set_Digit/10);
			else
				Unicode::strncpy(BTN_H_CorrectionBuffer, "---", 4); //buffer belongs to textArea
			presenter -> Corr_Sensor_Value(Selected, Set_Digit);
			if (Set_Digit == 0) FlagWriteH = 0; else FlagWriteH = 1;
			DigitWheelOff();
			break;	}
		case 4:		{	// установка корректировочного значения R
			if (Set_Digit != 0)
				Unicode::snprintfFloat(BTN_R_CorrectionBuffer, BTN_R_CORRECTION_SIZE, "%5.1f", (float)Set_Digit/10);
			else
				Unicode::strncpy(BTN_R_CorrectionBuffer, "---", 4); //buffer belongs to textArea
			presenter -> Corr_Sensor_Value(Selected, Set_Digit);
			if (Set_Digit == 0) FlagWriteR = 0; else FlagWriteR = 1;
			DigitWheelOff();
			break;	}
	}
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();

	if ((FlagWriteT == 1)||(FlagWriteH == 1)||(FlagWriteR == 1))
		BTNWrite.setVisible(true);
	else
		BTNWrite.setVisible(false);
	BTNWrite.invalidate();
	Settings6ViewBase::setupScreen();
}

void Settings6View::BTNCancelClicked()
{
	switch (Selected)
	{
		case 1:		{	// обработка выбора адреса
			SetAddress = SetAddressOld;
			scrollSensorAddress.setVisible(false);
			scrollSensorAddress.invalidate();
			break;	}
		default:	{
			DigitWheelOff();
			break;	}
	}
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

//Запуск колеса прокрутки "Выбор типа датчика"
void Settings6View::BTNSensorTypeClicked()
{
//	Selected = 0;
//	SetSensorOld = SetSensor;
//	scrollSensorType.setVisible(true);
//	scrollSensorType.invalidate();
//	BTNConfirm.setVisible(true);
//	BTNConfirm.invalidate();
//	BTNCancel.setVisible(true);
//	BTNCancel.invalidate();
//	BTNWrite.setVisible(false);
//	BTNWrite.invalidate();
}
//Запуск колеса прокрутки "Выбор адреса датчика"
void Settings6View::BTNSetAddressClicked()
{
	Selected = 1;
	FlagWriteT = 0;
	FlagWriteH = 0;
	FlagWriteR = 0;
	Unicode::strncpy(CurrentValue_TBuffer, "---", 4); //buffer belongs to textArea
	Unicode::strncpy(CurrentValue_HBuffer, "---", 4); //buffer belongs to textArea
	Unicode::strncpy(CurrentValue_RBuffer, "---", 4); //buffer belongs to textArea
	Unicode::strncpy(BTN_T_CorrectionBuffer, "---", 4); //buffer belongs to textArea
	Unicode::strncpy(BTN_H_CorrectionBuffer, "---", 4); //buffer belongs to textArea
	Unicode::strncpy(BTN_R_CorrectionBuffer, "---", 4); //buffer belongs to textArea

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
// Запуск выбора значения корректировки
void Settings6View::BTNSet_CORR_T()
{
	Selected = 2;
	CHNG_CORR_value_visual(false);
	CHNG_CORR_weels_visual(true);
	Settings6ViewBase::setupScreen();
	Print_Whole_Digit();
}

// Запуск выбора значения корректировки
void Settings6View::BTNSet_CORR_H()
{
	Selected = 3;
	CHNG_CORR_value_visual(false);
	CHNG_CORR_weels_visual(true);
	Settings6ViewBase::setupScreen();
	Print_Whole_Digit();
}

// Запуск выбора значения корректировки
void Settings6View::BTNSet_CORR_R()
{
	Selected = 4;
	CHNG_CORR_value_visual(false);
	CHNG_CORR_weels_visual(true);
	Settings6ViewBase::setupScreen();
	Print_Whole_Digit();
}

// Визуализация выбора значения корректировки
void Settings6View::CHNG_CORR_value_visual(bool visual)
{
	BTNSetAddress.setVisible(visual);
	BTNSensorType.setVisible(visual);
	LabelAddrSelect.setVisible(visual);
	LabelSensorTypeSelect.setVisible(visual);
	ChooseCorrection.setVisible(visual);

	LabelT.setVisible(visual);
	LabelH.setVisible(visual);
	LabelR.setVisible(visual);
	CurrentValue_T.setVisible(visual);
	CurrentValue_H.setVisible(visual);
	CurrentValue_R.setVisible(visual);
	BTN_T_Correction.setVisible(visual);
	BTN_H_Correction.setVisible(visual);
	BTN_R_Correction.setVisible(visual);
	Settings6ViewBase::setupScreen();
}

// Визуализация колеса прокрутки
void Settings6View::CHNG_CORR_weels_visual(bool visual)
{
	switch (Selected) {
		case 2:		{// T
			LabelT_1.setVisible(visual);
			LabelT_1_1.setVisible(visual);
			break;	}
		case 3:		{	// H
			LabelH_1.setVisible(visual);
			LabelH_1_1.setVisible(visual);
			break;	}
		case 4:		{// R
			LabelR_1.setVisible(visual);
			LabelR_1_1.setVisible(visual);
			break;	}
		default:
			break;
	}

	scrollWheelDigit1.setVisible(visual);
	scrollWheelDigit2.setVisible(visual);
	scrollWheelDigit3.setVisible(visual);
	scrollWheelDigit4.setVisible(visual);
	Whole_Digit.setVisible(visual);
	BTNConfirm.setVisible(visual);
	BTNCancel.setVisible(visual);

	if (visual == false)
	{
		// включим видимость кнопки Reset для датчика типа 1
		if (Sensor_array[SetAddress].TypeOfSensor == 1)
			BTNReset.setVisible(true);
		else
			BTNReset.setVisible(false);
	} else {
		BTNReset.setVisible(false);
	}

	Whole_Digit.invalidate();
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNReset.invalidate();
	Settings6ViewBase::setupScreen();
}


// Вывод на печать подобранного колёсами прокрутки значения числа
void Settings6View::Print_Whole_Digit()
{
	Set_Digit = Digit1 + Digit2*10 + Digit3*100 + Digit4*1000;
	Unicode::snprintfFloat(Whole_DigitBuffer, sizeof(Whole_DigitBuffer), "%05.1f", (float)Set_Digit/10);
	Whole_Digit.invalidate();
}

// отображение считанных из датчика значений
void Settings6View::CorrData_T_View()
{
	if (Model::T_CORR_sensor != 0)
		Unicode::snprintfFloat(CurrentValue_TBuffer, sizeof(CurrentValue_TBuffer), "%5.1f", (float)Model::T_CORR_sensor/10);
	else
		Unicode::strncpy(CurrentValue_TBuffer, "---", 4); //buffer belongs to textArea
	CurrentValue_T.invalidate();
}
void Settings6View::CorrData_H_View()
{
	SwitchHRvisible();
	if (Model::H_CORR_sensor != 0)
		Unicode::snprintfFloat(CurrentValue_HBuffer, sizeof(CurrentValue_HBuffer), "%5.1f", (float)Model::H_CORR_sensor/10);
	else
		Unicode::strncpy(CurrentValue_HBuffer, "---", 4); //buffer belongs to textArea
	CurrentValue_H.invalidate();
}
void Settings6View::CorrData_R_View()
{
	SwitchHRvisible();
	if (Model::R_CORR_sensor != 0)
		Unicode::snprintfFloat(CurrentValue_RBuffer, sizeof(CurrentValue_RBuffer), "%5.1f", (float)Model::R_CORR_sensor/10);
	else
		Unicode::strncpy(CurrentValue_RBuffer, "---", 4); //buffer belongs to textArea
	CurrentValue_R.invalidate();
}

void Settings6View::SwitchHRvisible()
{
	if (Selected == 0)
	{
		switch (Model::Type_CORR_sensor) {
			case 1:		{
				LabelH.setVisible(true);
				LabelR.setVisible(false);
				CurrentValue_H.setVisible(true);
				CurrentValue_R.setVisible(false);
				BTN_H_Correction.setVisible(true);
				BTN_R_Correction.setVisible(false);
				break;	}
			case 2:		{
				LabelH.setVisible(false);
				LabelR.setVisible(true);
				CurrentValue_H.setVisible(false);
				CurrentValue_R.setVisible(true);
				BTN_H_Correction.setVisible(false);
				BTN_R_Correction.setVisible(true);
				break;	}
			default:
				break;
		}
		CurrentValue_H.invalidate();
		CurrentValue_R.invalidate();
		BTN_H_Correction.invalidate();
		BTN_R_Correction.invalidate();
	}
}

void Settings6View::DigitWheelOff()
{
	CHNG_CORR_value_visual(true);
	CHNG_CORR_weels_visual(false);
	Selected = 0;
	SwitchHRvisible();
}


