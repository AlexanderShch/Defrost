#include <gui/settings5_screen/Settings5View.hpp>

uint8_t Selected;
int16_t SetSensor;
int SetSpeed;
int Selected;
/* Definitions for ProgrammingSens */
osThreadId_t ProgrammingSensHandle;
const osThreadAttr_t ProgrammingSens_attributes = {
  .name = "ProgrammingSens",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
int16_t SetAddress;

void StartProgrammingSensor(void *argument);
extern void ProgrammingSensor(void);

Settings5View::Settings5View()
extern SENSOR_typedef_t Sensor_array[SQ];

Settings5View::Settings5View():
		//Вызов функций Handler для Callback
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

    //Callback для передачи ItemSelected значения при окончании анимации скролла
    scrollSensorType.setItemSelectedCallback(scrollSensorTypeItemSelectedCallback);
    scrollSensorSpeedNew.setItemSelectedCallback(scrollSensorSpeedNewItemSelectedCallback);
    scrollSensorAddressNew.setItemSelectedCallback(scrollSensorAddressNewItemSelectedCallback);
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
//Запись значений в Scroll SensorType
void Settings5View::scrollSensorTypeUpdateItem(ScrollItemContainer& item, int16_t itemIndex)
{
	item.updateScrollItem(itemIndex);
}

//Запись значений в SensorType и SetSensor для передачи в кнопку при нажатии "Записать"
void Settings5View::scrollSensorTypeUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex)
{
	item.updateScrollSelectedItem(itemIndex);
}

//Запись значений в scrollSensorSpeedNew
void Settings5View::scrollSensorSpeedNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex)
{
    switch (itemIndex)
    {
    case 0:
    	item.updateScrollItem(2400);
        break;
    case 1:
    	item.updateScrollItem(4800);
        break;
    case 2:
    	item.updateScrollItem(9600);
        break;
    case 3:
    	item.updateScrollItem(19200);
        break;
    case 4:
    	item.updateScrollItem(38400);
        break;
    case 5:
    	item.updateScrollItem(57600);
        break;
    case 6:
    	item.updateScrollItem(115200);
        break;
    case 7:
    	item.updateScrollItem(1200);
        break;
    }
}

//Запись значений в scrollSensorSpeedNewCenterItem
void Settings5View::scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex)
{
    switch (itemIndex)
    {
    case 0:
    	item.updateScrollSelectedItem(2400);
        break;
    case 1:
    	item.updateScrollSelectedItem(4800);
        break;
    case 2:
    	item.updateScrollSelectedItem(9600);
        break;
    case 3:
    	item.updateScrollSelectedItem(19200);
        break;
    case 4:
    	item.updateScrollSelectedItem(38400);
        break;
    case 5:
    	item.updateScrollSelectedItem(57600);
        break;
    case 6:
    	item.updateScrollSelectedItem(115200);
        break;
    case 7:
    	item.updateScrollSelectedItem(1200);
        break;
    }
}

//Запись значений в SensorAddress
void Settings5View::scrollSensorAddressNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex)
{
    item.updateScrollItem(Sensor_array[itemIndex].Address);
}

//Запись значений в SensorAdress Center Item
void Settings5View::scrollSensorAddressNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex)
{
	item.updateScrollSelectedItem(Sensor_array[itemIndex].Address);
}

void Settings5View::scrollSensorTypeItemSelectedHandler(int16_t itemSelected)
{
	SetSensor = itemSelected;
}

void Settings5View::scrollSensorSpeedNewItemSelectedHandler(int16_t itemSelected)
{
    switch (itemSelected)
    {
    case 0:
        SetSpeed = 2400;
        break;
    case 1:
    	SetSpeed = 4800;
        break;
    case 2:
    	SetSpeed = 9600;
        break;
    case 3:
    	SetSpeed = 19200;
        break;
    case 4:
    	SetSpeed = 38400;
        break;
    case 5:
    	SetSpeed = 57600;
        break;
    case 6:
    	SetSpeed = 115200;
        break;
    case 7:
    	SetSpeed = 1200;
        break;
    }
}

void Settings5View::scrollSensorAddressNewItemSelectedHandler(int16_t itemSelected)
{
	SetAddress = Sensor_array[itemSelected].Address;
}

//Запись выбранных значений скорости и адреса в датчик
void Settings5View::BTNWriteClicked()
{

}

//Выбор типа датчика
void Settings5View::BTNSensorTypeClicked()
{
	scrollSensorType.setVisible(true);
	Selected = 0;
	BTNConfirm.setVisible(true);
	BTNCancel.setVisible(true);
	BTNWrite.setVisible(false);
	scrollSensorType.invalidate();
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNWrite.invalidate();
}

//Выбор скорости передачи датчика
void Settings5View::BTNSetSpeedClicked()
{
	scrollSensorSpeedNew.setVisible(true);
	Selected = 1;
	BTNConfirm.setVisible(true);
	BTNCancel.setVisible(true);
	BTNWrite.setVisible(false);
	scrollSensorSpeedNew.invalidate();
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNWrite.invalidate();
}

//Выбор нового адреса датчика
void Settings5View::BTNSetAddressClicked()
{
	scrollSensorAddressNew.setVisible(true);
	Selected = 2;
	BTNConfirm.setVisible(true);
	BTNCancel.setVisible(true);
	BTNWrite.setVisible(false);
	scrollSensorAddressNew.invalidate();
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNWrite.invalidate();
}

//Кнопка подтверждения выбранного значения
void Settings5View::BTNConfirmClicked()
{
	switch (Selected)
	{
		case 0:
		Unicode::snprintf(BTNSensorTypeBuffer, BTNSENSORTYPE_SIZE, "%d", SetSensor);
		scrollSensorType.setVisible(false);
		break;
		case 1:
		Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", SetSpeed);
		scrollSensorSpeedNew.setVisible(false);
		break;
		case 2:
		BTNSetAddress.invalidate();
		Unicode::snprintf(BTNSetAddressBuffer, BTNSETADDRESS_SIZE, "%d", SetAddress);
		scrollSensorAddressNew.setVisible(false);
		break;
	}
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNWrite.setVisible(true);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNWrite.invalidate();
	Settings5ViewBase::setupScreen();
}

//Кнопка отмены выбора значения
void Settings5View::BTNCancelClicked()
{
	switch (Selected)
	{
		case 0:
		scrollSensorType.setVisible(false);
		break;
		case 1:
		scrollSensorSpeedNew.setVisible(false);
		break;
		case 2:
		scrollSensorAddressNew.setVisible(false);
		break;
	}
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNWrite.setVisible(true);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNWrite.invalidate();
	Settings5ViewBase::setupScreen();
}
