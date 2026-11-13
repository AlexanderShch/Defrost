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

extern void ProgrammingSensor_Start(void);
extern void ProgrammingSensor_RequestStop(void);
extern SENSOR_typedef_t Sensor_array[SQ];
extern uint8_t SensNullValue;
extern int BaudRate_Type1[];	// список скоростей для типа 1
extern int BaudRate_Type2[];	// список скоростей для типа 2
extern int BaudRate_Type5[];
extern int BaudRate_Type4[];	// список скоростей для типа 4

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

    //В контейнерах колёс прокрутки устанавливаются Callback функции обработки события
    //например, для передачи ItemSelected значения при окончании анимации скролла при выборе значения в колесе
    scrollSensorType.setItemSelectedCallback(scrollSensorTypeItemSelectedCallback);
    scrollSensorSpeedNew.setItemSelectedCallback(scrollSensorSpeedNewItemSelectedCallback);
    scrollSensorAddressNew.setItemSelectedCallback(scrollSensorAddressNewItemSelectedCallback);

    writeErrorMessage.setTypedText(SensorCurrentSpeed.getTypedText());
    writeErrorMessage.setWildcard(writeErrorBuffer);
    writeErrorMessage.setPosition(20, 70, 200, 100);
    writeErrorMessage.setWideTextAction(touchgfx::WIDE_TEXT_WORDWRAP);
    writeErrorMessage.setColor(Alert_Message.getColor());
    writeErrorMessage.setVisible(false);
    add(writeErrorMessage);
    MB_Error_t previousResult;
    (void)ProgrammingSensor_TakeWriteResult(&previousResult);
    scrollSensorType.setNumberOfItems(STQ);
    FlagWrite_visible = 0;
    SetSpeed = SensNullValue;
    SetAddress = 0;
	SetSensor = 0;
    UpdateDestinationChoices();
	presenter -> PR_Sensor_Type(SetSensor);
	// Сканирование начнётся только после выбора типа датчика оператором.
	ProgrammingSensor_Start();
}

void Settings5View::tearDownScreen()
{
    Settings5ViewBase::tearDownScreen();
    ProgrammingSensor_RequestStop();
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
    if (itemIndex < 0 || itemIndex >= SensorConfig_BaudCount((uint8_t)SetSensor))
    { item.updateScrollItem(0); return; }
	switch (SetSensor) {
		case 1:		{
			item.updateScrollItem(BaudRate_Type1[itemIndex]);
			break;	}
		case 2:		{
			item.updateScrollItem(BaudRate_Type2[itemIndex]);
			break;	}
		case 5:		{
			item.updateScrollItem(BaudRate_Type5[itemIndex]);
			break;	}
		case 4:		{
			item.updateScrollItem(BaudRate_Type4[itemIndex]);
			break;	}
		default:	{
			item.updateScrollItem(0);
			break;	}
	}
}

//Заполнение фокусного значения BaudRate в колесе прокрутки после остановки прокрутки даже после нажатия "Записать" в колесе
void Settings5View::scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex)
{
    if (itemIndex < 0 || itemIndex >= SensorConfig_BaudCount((uint8_t)SetSensor))
    { item.updateScrollSelectedItem(0); return; }
	switch (SetSensor) {
		case 1:		{
			item.updateScrollSelectedItem(BaudRate_Type1[itemIndex]);
			break;	}
		case 2:		{
			item.updateScrollSelectedItem(BaudRate_Type2[itemIndex]);
			break;	}
		case 5:		{
			item.updateScrollSelectedItem(BaudRate_Type5[itemIndex]);
			break;	}
		case 4:		{
			item.updateScrollSelectedItem(BaudRate_Type4[itemIndex]);
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
    const uint8_t channel = (itemIndex < 0) ? SQ :
        SensorConfig_DestinationChannel((uint8_t)SetSensor, (uint8_t)itemIndex);
    if (channel >= SQ) { char empty[] = "---"; item.updateTextItem(empty, 0); return; }
    char text[20];
    strcpy(text, Sensor_array[channel].PositionName);
    strcat(text, " %d");
    item.updateTextItem(text, Sensor_array[channel].Address);
}
//Заполнение фокусного значения SensorAddress в колесе прокрутки после остановки прокрутки даже после нажатия "Записать" в колесе
void Settings5View::scrollSensorAddressNewUpdateCenterItem(TextSelectedContainer& item, int16_t itemIndex)
{
    const uint8_t channel = (itemIndex < 0) ? SQ :
        SensorConfig_DestinationChannel((uint8_t)SetSensor, (uint8_t)itemIndex);
    if (channel >= SQ) { char empty[] = "---"; item.updateTextSelectedItem(empty, 0); return; }
    char text[20];
    strcpy(text, Sensor_array[channel].PositionName);
    strcat(text, " %d");
    item.updateTextSelectedItem(text, Sensor_array[channel].Address);
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
    const uint8_t channel = (itemSelected < 0) ? SQ :
        SensorConfig_DestinationChannel((uint8_t)SetSensor, (uint8_t)itemSelected);
    if (channel < SQ) SetAddress = Sensor_array[channel].Address;
}

//Запись выбранных значений скорости и адреса в датчик при нажатии "Запись" в окне Settings5
void Settings5View::BTNWriteClicked()
{
    if (SensorConfig_DestinationCount((uint8_t)SetSensor) == 0u || SetAddress == 0u ||
        SetSpeed >= SensorConfig_BaudCount((uint8_t)SetSensor)) return;
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
	// У Taidacent своя таблица из семи скоростей, без 115200 бод.
	// Размер колеса и допустимость сохранённого индекса должны соответствовать типу.
	// Индекс 0 означает допустимые 1200 бод; это не признак отсутствия выбора.
	scrollSensorSpeedNew.setNumberOfItems(SetSensor == 5 ? TYPE5_BAUD_RATE_NUMBER : BAUD_RATE_NUMBER);
	if (SetSpeed >= (SetSensor == 5 ? TYPE5_BAUD_RATE_NUMBER : BAUD_RATE_NUMBER))
		SetSpeed = 0;
	scrollSensorSpeedNew.animateToItem(SetSpeed, 0);
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
    const uint8_t count = SensorConfig_DestinationCount((uint8_t)SetSensor);
    if (count == 0u) return;
	Selected = 2;
	SetAddressOld = SetAddress;
    uint8_t selectedItem = 0u;
    for (uint8_t item = 0u; item < count; ++item)
    {
        const uint8_t channel = SensorConfig_DestinationChannel((uint8_t)SetSensor, item);
        if (Sensor_array[channel].Address == SetAddress) selectedItem = item;
    }
    scrollSensorAddressNew.animateToItem(selectedItem, 0);
    scrollSensorAddressNewItemSelectedHandler(selectedItem);
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
			if (SetSensor != SetSensorOld)
			{
				SetSpeed = SensNullValue;
				FlagWrite_visible = 0;
				Unicode::strncpy(BTNSetSpeedBuffer, "---", BTNSETSPEED_SIZE);
				Model::setCurrentVal_PR(SensNullValue, SensNullValue);
                UpdateDestinationChoices();
			}
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
				case 5:		{
					Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", BaudRate_Type5[SetSpeed]);
					break;	}
				case 4:		{
					Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", BaudRate_Type4[SetSpeed]);
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
			writeErrorMessage.setVisible(false);
			BTNConfirm.setVisible(false);
			Alert_Background.invalidate();
			Alert_Message.invalidate();
			break;
	}
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	if ((FlagWrite_visible == 1)&&(SetSpeed < (SetSensor == 5 ? TYPE5_BAUD_RATE_NUMBER : BAUD_RATE_NUMBER))&&(SetAddress != 0))
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
	if ((FlagWrite_visible == 1)&&(SetSpeed < (SetSensor == 5 ? TYPE5_BAUD_RATE_NUMBER : BAUD_RATE_NUMBER))&&(SetAddress != 0))
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
		if ((FlagWrite_visible == 1)&&(SetSpeed < (SetSensor == 5 ? TYPE5_BAUD_RATE_NUMBER : BAUD_RATE_NUMBER))&&(SetAddress != 0))
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
	if (Alert_Background.isVisible()) BTNWrite.setVisible(false);
	BTNWrite.invalidate();
	SensorCurrentAddress.invalidate();
}

// Вывод на экран значения скорости из Модели
void Settings5View::Val_BaudRate_UpdateView(uint8_t Val)
{
	if (Val < (SetSensor == 5 ? TYPE5_BAUD_RATE_NUMBER : BAUD_RATE_NUMBER))
	{
		switch (SetSensor) {
			case 1:		{
				Unicode::snprintf(SensorCurrentSpeedBuffer, sizeof(SensorCurrentSpeedBuffer), "%d", BaudRate_Type1[Val]);
				break;	}
			case 2:		{
				Unicode::snprintf(SensorCurrentSpeedBuffer, sizeof(SensorCurrentSpeedBuffer), "%d", BaudRate_Type2[Val]);
				break;	}
			case 5:		{
				Unicode::snprintf(SensorCurrentSpeedBuffer, sizeof(SensorCurrentSpeedBuffer), "%d", BaudRate_Type5[Val]);
				break;	}
			case 4:		{
				Unicode::snprintf(SensorCurrentSpeedBuffer, sizeof(SensorCurrentSpeedBuffer), "%d", BaudRate_Type4[Val]);
				break;	}
			default:
				break;
		}
		FlagWrite_visible = 1;
		if ((FlagWrite_visible == 1)&&(SetSpeed < (SetSensor == 5 ? TYPE5_BAUD_RATE_NUMBER : BAUD_RATE_NUMBER))&&(SetAddress != 0))
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
	if (Alert_Background.isVisible()) BTNWrite.setVisible(false);
	BTNWrite.invalidate();
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


void Settings5View::WriteError_Output(MB_Error_t error)
{
    Selected = 3;
    const uint8_t stage = ProgrammingSensor_GetWriteStage();
    if (error == MB_ERROR_EEPROM)
    {
        const SensorEepromDiagnostic diagnostic = ProgrammingSensor_GetEepromDiagnostic();
        Unicode::snprintf(writeErrorBuffer, 64, "error 8-%d\nHAL %d\nI2C %d",
            (int)diagnostic.stage, (int)diagnostic.halStatus, (int)diagnostic.i2cError);
    }
    else if (error != MB_ERROR_NO && stage != 0u)
        Unicode::snprintf(writeErrorBuffer, 24, "error\n%d-%d", (int)error, (int)stage);
    else
        Unicode::snprintf(writeErrorBuffer, 24, "error\n%d", (int)error);
    Alert_Background.setVisible(true);
    Alert_Message.setVisible(false);
    writeErrorMessage.setVisible(true);
    BTNConfirm.setVisible(true);
    BTNCancel.setVisible(false);
    BTNWrite.setVisible(false);
    invalidate();
}

void Settings5View::WriteSaved_Output()
{
    WriteError_Output(MB_ERROR_NO);
    Model::Flag_Alert = 0;
    // Тип 5 уже программно перезапущен и проверен в Type5_Program.
    // Оператору остаётся перезапустить контроллер для применения сохранённых
    // назначений датчиков; для остальных типов сохраняем прежнюю инструкцию.
    const char *message = (Model::Type_of_sensor == 5)
        ? "Записано\nПерезапустите\nконтроллер"
        : "Записано\nПерезапустите\nдатчик и\nконтроллер";
    if (ProgrammingSensor_Needs19200() != 0u) message = "Записано, но нужно 19200";
    Unicode::fromUTF8((const uint8_t *)message, writeErrorBuffer, 64);
    writeErrorMessage.invalidate();
}

void Settings5View::UpdateDestinationChoices()
{
    const uint8_t count = SensorConfig_DestinationCount((uint8_t)SetSensor);
    scrollSensorAddressNew.setVisible(false);
    BTNSetAddress.setVisible(count != 0u);
    if (count != 0u)
    {
        scrollSensorAddressNew.setNumberOfItems(count);
        scrollSensorAddressNew.animateToItem(0, 0);
    }
    const uint8_t baudCount = SensorConfig_BaudCount((uint8_t)SetSensor);
    if (baudCount != 0u) scrollSensorSpeedNew.setNumberOfItems(baudCount);
    BTNSetSpeed.setVisible(baudCount != 0u);
    SetSpeed = SetSpeedOld = SensNullValue;
    Unicode::strncpy(BTNSetSpeedBuffer, "---", BTNSETSPEED_SIZE);
    BTNSetSpeed.invalidate();
    // A type change invalidates the previous destination; require a fresh confirmation.
    SetAddress = SetAddressOld = 0u;
    Unicode::strncpy(BTNSetAddressBuffer, "---", BTNSETADDRESS_SIZE);
    BTNSetAddress.invalidate();
    scrollSensorAddressNew.invalidate();
}
