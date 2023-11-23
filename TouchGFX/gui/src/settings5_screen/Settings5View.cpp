#include <gui/settings5_screen/Settings5View.hpp>
#include "string.h"

extern SENSOR_Type_t Sensor_type[STQ];
extern SENSOR_typedef_t Sensor_array[SQ];

    uint8_t Selected;				// нажата кнопка "Принять": 0 - тип, 1 - скорость, 2 - адрес, 3 - предупреждение
    int16_t SetSensor = 0;			// вновь устанавливаемое значение типа датчика, выводится на экран
    int16_t SetSensorOld = 0;		// старое значение типа датчика, сохраняется на время выбора в колесе нового значения
    uint8_t SetSpeed;
    uint8_t SetSpeedOld;
    uint8_t SetAddress;
    uint8_t SetAddressOld;
    uint8_t FlagWrite_visible = 0;	// флаг разрешения видимости кнопки "Запись" в датчик

    /* Definitions for ProgrammingSens */
osThreadId_t ProgrammingSensHandle;
const osThreadAttr_t ProgrammingSens_attributes = {
  .name = "ProgrammingSens",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

void StartProgrammingSensor(void *argument);
extern void ProgrammingSensor(void);
extern SENSOR_typedef_t Sensor_array[SQ];
extern uint8_t SensNullValue;
extern int BaudRate_Type1[];
extern int BaudRate_Type2[];

Settings5View::Settings5View():
	//Создание смарт-пойнтеров на функции Handler для Callback
	scrollSensorTypeItemSelectedCallback(this, &Settings5View::scrollSensorTypeItemSelectedHandler),
	scrollSensorSpeedNewItemSelectedCallback(this, &Settings5View::scrollSensorSpeedNewItemSelectedHandler),
	scrollSensorAddressNewItemSelectedCallback(this, &Settings5View::scrollSensorAddressNewItemSelectedHandler)
{
}



void Settings5View::setupScreen()
{
    Settings5ViewBase::setupScreen();
    /* creation of ProgrammingSens task */
    ProgrammingSensHandle = osThreadNew(StartProgrammingSensor, NULL, &ProgrammingSens_attributes);

    //В контейнерах колёс прокрутки устанавливаются Callback функции обработки события
    //например, для передачи ItemSelected значения при окончании анимации скролла при выборе значения в колесе
    scrollSensorType.setItemSelectedCallback(scrollSensorTypeItemSelectedCallback);
    scrollSensorSpeedNew.setItemSelectedCallback(scrollSensorSpeedNewItemSelectedCallback);
    scrollSensorAddressNew.setItemSelectedCallback(scrollSensorAddressNewItemSelectedCallback);

    FlagWrite_visible = 0;
    SetSpeed = 0;
    SetAddress = 0;
	SetSensor = 0;
	presenter -> PR_Sensor_Type(SetSensor);

}

void Settings5View::tearDownScreen()
{
    Settings5ViewBase::tearDownScreen();
    /* delete ProgrammingSens task */
    osThreadTerminate(ProgrammingSensHandle);
}

void StartProgrammingSensor(void *argument)
{
	ProgrammingSensor();
}

//Запись значений в ScrollItemContainer и обновление на кнопке BTNSetSpeed
//Заполнение колеса прокрутки значениями SensorType, в т.ч. при прокрутке колеса
//Вызывается для каждого значения в массиве значений колеса
void Settings5View::scrollSensorTypeUpdateItem(TextContainer& item, int16_t itemIndex)
{
//	item.updateScrollItem(itemIndex);
	//	Формирование строки с именем типа датчика и его идентификатором
	char str[20];
	char FormatText[] = " %d";
	strcpy(str, Sensor_type[itemIndex].TypeName);		// копирование PositionName в начало строки str, включая символ окончания строки \0
	strcat(str, FormatText);							// добавление символа форматирования в строку str
	item.updateTextItem(str, Sensor_type[itemIndex].TypeNumber);
}

//Заполнение фокусного значения SensorType в колесе прокрутки после остановки прокрутки даже после нажатия "Записать" в колесе
void Settings5View::scrollSensorTypeUpdateCenterItem(TextSelectedContainer& item, int16_t itemIndex)
{
//	item.updateScrollSelectedItem(itemIndex);
	//	Формирование строки с именем типа датчика и его идентификатором
	char str[20];
	char FormatText[] = " %d";
	strcpy(str, Sensor_type[itemIndex].TypeName);		// копирование PositionName в начало строки str, включая символ окончания строки \0
	strcat(str, FormatText);							// добавление символа форматирования в строку str
	item.updateTextSelectedItem(str, Sensor_type[itemIndex].TypeNumber);
}

//Запись значений в scrollSensorSpeedNew
//Заполнение колеса прокрутки значениями BaudRate, в т.ч. при прокрутке колеса
//Вызывается для каждого значения в массиве значений колеса
void Settings5View::scrollSensorSpeedNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex)
{
	switch (SetSensor) {
		case 1:		{
			item.updateScrollItem(BaudRate_Type1[itemIndex]);
			break;	}
		case 2:		{
			item.updateScrollItem(BaudRate_Type2[itemIndex]);
			break;	}
		default:	{
			item.updateScrollItem(0);
			break;	}
	}
}

//Заполнение фокусного значения BaudRate в колесе прокрутки после остановки прокрутки даже после нажатия "Записать" в колесе
void Settings5View::scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex)
{
	switch (SetSensor) {
		case 1:		{
			item.updateScrollSelectedItem(BaudRate_Type1[itemIndex]);
			break;	}
		case 2:		{
			item.updateScrollSelectedItem(BaudRate_Type2[itemIndex]);
			break;	}
		default:	{
			item.updateScrollSelectedItem(0);
			break;	}
	}
}

//Запись значений в SensorAddress
//Заполнение колеса прокрутки значениями Sensor_array[itemIndex].Address, в т.ч. при прокрутке колеса
//Вызывается для каждого значения в массиве значений колеса
void Settings5View::scrollSensorAddressNewUpdateItem(TextContainer& item, int16_t itemIndex)
{
//    item.updateScrollItem(Sensor_array[itemIndex].Address);
	//	Формирование строки с именем датчика и его адресом
	char str[20];
	char FormatText[] = " %d";
	strcpy(str, Sensor_array[itemIndex].PositionName);	// копирование PositionName в начало строки str, включая символ окончания строки \0
	strcat(str, FormatText);							// добавление символа форматирования в строку str
	item.updateTextItem(str, Sensor_array[itemIndex].Address);
}
//Заполнение фокусного значения SensorAddress в колесе прокрутки после остановки прокрутки даже после нажатия "Записать" в колесе
void Settings5View::scrollSensorAddressNewUpdateCenterItem(TextSelectedContainer& item, int16_t itemIndex)
{
//	item.updateScrollSelectedItem(Sensor_array[itemIndex].Address);
	//	Формирование строки с именем типа датчика и его идентификатором
	char str[20];
	char FormatText[] = " %d";
	strcpy(str, Sensor_array[itemIndex].PositionName);	// копирование PositionName в начало строки str, включая символ окончания строки \0
	strcat(str, FormatText);							// добавление символа форматирования в строку str
	item.updateTextSelectedItem(str, Sensor_array[itemIndex].Address);
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings5View::scrollSensorTypeItemSelectedHandler(int16_t itemSelected)
{
	SetSensor = itemSelected;
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings5View::scrollSensorSpeedNewItemSelectedHandler(int16_t itemSelected)
{
	SetSpeed = itemSelected;
}
// Вызывается при установке значения в центральное положение в колесе, в фокус
void Settings5View::scrollSensorAddressNewItemSelectedHandler(int16_t itemSelected)
{
	SetAddress = Sensor_array[itemSelected].Address;
}

//Запись выбранных значений скорости и адреса в датчик при нажатии "Запись" в окне Settings5
void Settings5View::BTNWriteClicked()
{
	presenter -> PR_Sensor_Data_Write(SetSpeed, SetAddress);

}

//Запуск колеса прокрутки "Выбор типа датчика"
void Settings5View::BTNSensorTypeClicked()
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

//Запуск колеса прокрутки "Выбор скорости передачи датчика"
void Settings5View::BTNSetSpeedClicked()
{
	Selected = 1;
	SetSpeedOld = SetSpeed;
	scrollSensorSpeedNew.setVisible(true);
	scrollSensorSpeedNew.invalidate();
	BTNConfirm.setVisible(true);
	BTNConfirm.invalidate();
	BTNCancel.setVisible(true);
	BTNCancel.invalidate();
	BTNWrite.setVisible(false);
	BTNWrite.invalidate();
}

//Запуск колеса прокрутки "Выбор адреса датчика"
void Settings5View::BTNSetAddressClicked()
{
	Selected = 2;
	SetAddressOld = SetAddress;
	scrollSensorAddressNew.setVisible(true);
	scrollSensorAddressNew.invalidate();
	BTNConfirm.setVisible(true);
	BTNConfirm.invalidate();
	BTNCancel.setVisible(true);
	BTNCancel.invalidate();
	BTNWrite.setVisible(false);
	BTNWrite.invalidate();
}

//Кнопка подтверждения "Принять"
void Settings5View::BTNConfirmClicked()
{
	switch (Selected)
	{
		case 0:		// обработка выбора типа датчика
			SetSensorOld = SetSensor;
			Unicode::snprintf(BTNSensorTypeBuffer, BTNSENSORTYPE_SIZE, "%d", SetSensor);
			scrollSensorType.setVisible(false);
			presenter -> PR_Sensor_Type(SetSensor);
			break;
		case 1:		// обработка выбора скорости датчика
			SetSpeedOld = SetSpeed;
			switch (SetSensor) {
				case 1:		{
					Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", BaudRate_Type1[SetSpeed]);
					break;	}
				case 2:		{
					Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", BaudRate_Type2[SetSpeed]);
					break;	}
				default:
					break;
			}
			scrollSensorSpeedNew.setVisible(false);
			break;
		case 2:		// обработка выбора адреса датчика
			SetAddressOld = SetAddress;
			Unicode::snprintf(BTNSetAddressBuffer, BTNSETADDRESS_SIZE, "%d", SetAddress);
			BTNSetAddress.invalidate();
			scrollSensorAddressNew.setVisible(false);
			break;
		case 3:		// обработка подтверждения прочтения предупреждения
			Model::Flag_Alert = 0;
			Alert_Background.setVisible(false);
			Alert_Message.setVisible(false);
			BTNConfirm.setVisible(false);
			Alert_Background.invalidate();
			Alert_Message.invalidate();
			break;
	}
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	if ((FlagWrite_visible == 1)&&(SetSpeed != 0)&&(SetAddress != 0))
		BTNWrite.setVisible(true);
	else
		BTNWrite.setVisible(false);
	BTNWrite.invalidate();
	Settings5ViewBase::setupScreen();
}

//Кнопка отмены выбора значения в колесе прокрутки
void Settings5View::BTNCancelClicked()
{
	switch (Selected)
	{
		case 0:
			SetSensor = SetSensorOld;
			scrollSensorType.setVisible(false);
			break;
		case 1:
			SetSpeed = SetSpeedOld;
			scrollSensorSpeedNew.setVisible(false);
			break;
		case 2:
			SetAddress = SetAddressOld;
			scrollSensorAddressNew.setVisible(false);
			break;
	}
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	if ((FlagWrite_visible == 1)&&(SetSpeed != 0)&&(SetAddress != 0))
		BTNWrite.setVisible(true);
	else
		BTNWrite.setVisible(false);
	BTNWrite.invalidate();
	Settings5ViewBase::setupScreen();
}

// Вывод на экран значения адреса из Модели
void Settings5View::Val_Addr_UpdateView(uint8_t Val)
{
	if (Val != SensNullValue)
	{
		Unicode::snprintf(SensorCurrentAddressBuffer, sizeof(SensorCurrentAddressBuffer), "%d", Val);
		FlagWrite_visible = 1;
		if ((FlagWrite_visible == 1)&&(SetSpeed != 0)&&(SetAddress != 0))
			BTNWrite.setVisible(true);
		else
			BTNWrite.setVisible(false);
	}
	else
	{
		Unicode::strncpy(SensorCurrentAddressBuffer, "---", 4); //buffer belongs to textArea
		FlagWrite_visible = 0;
		BTNWrite.setVisible(false);
	}
	BTNWrite.invalidate();
	SensorCurrentAddress.invalidate();
}

// Вывод на экран значения скорости из Модели
void Settings5View::Val_BaudRate_UpdateView(uint8_t Val)
{
	if (Val != SensNullValue)
	{
		switch (SetSensor) {
			case 1:		{
				Unicode::snprintf(SensorCurrentSpeedBuffer, sizeof(SensorCurrentSpeedBuffer), "%d", BaudRate_Type1[Val]);
				break;	}
			case 2:		{
				Unicode::snprintf(SensorCurrentSpeedBuffer, sizeof(SensorCurrentSpeedBuffer), "%d", BaudRate_Type2[Val]);
				break;	}
			default:
				break;
		}
		FlagWrite_visible = 1;
		if ((FlagWrite_visible == 1)&&(SetSpeed != 0)&&(SetAddress != 0))
			BTNWrite.setVisible(true);
		else
			BTNWrite.setVisible(false);
	}
	else
	{
		Unicode::strncpy(SensorCurrentSpeedBuffer, "---", 4); //buffer belongs to textArea
		FlagWrite_visible = 0;
		BTNWrite.setVisible(false);
	}
	SensorCurrentSpeed.invalidate();
}

// Выод на экран сообщения о необходимости сбросить питание модуля
void Settings5View::Alert_Message_Output()
{
	Selected = 3;
	Alert_Background.setVisible(true);
	Alert_Message.setVisible(true);
	BTNConfirm.setVisible(true);
	BTNWrite.setVisible(false);
	Alert_Background.invalidate();
	Alert_Message.invalidate();
	BTNConfirm.invalidate();
	BTNWrite.invalidate();
}

