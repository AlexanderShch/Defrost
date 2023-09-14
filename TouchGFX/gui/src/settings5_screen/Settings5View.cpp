#include <gui/settings5_screen/Settings5View.hpp>

int SetSpeed;
int Selected;

Settings5View::Settings5View()
{

}

void Settings5View::setupScreen()
{
    Settings5ViewBase::setupScreen();
}

void Settings5View::tearDownScreen()
{
    Settings5ViewBase::tearDownScreen();
}

//Запись значений в ScrollItemContainer и обновление на кнопке BTNSetSpeed
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
    	item.updateScrollItem(115200);
        break;
    }
}

//Запись значений в ScrollSelectedItem и переменную SetSpeed (почему-то идет сдвиг на -1)
void Settings5View::scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex)
{
    switch (itemIndex)
    {
    case 0:
        item.updateScrollSelectedItem(2400);
        SetSpeed = 115200;
        break;
    case 1:
    	item.updateScrollSelectedItem(4800);
    	SetSpeed = 2400;
        break;
    case 2:
    	item.updateScrollSelectedItem(9600);
    	SetSpeed = 4800;
        break;
    case 3:
    	item.updateScrollSelectedItem(19200);
    	SetSpeed = 9600;
        break;
    case 4:
    	item.updateScrollSelectedItem(38400);
    	SetSpeed = 19200;
        break;
    case 5:
    	item.updateScrollSelectedItem(115200);
    	SetSpeed = 38400;
        break;
    }
}

//Запись выбранных значений скорости и адреса в датчик
void Settings5View::BTNWriteClicked()
{

}

//Выбор скорости передачи датчика
void Settings5View::BTNSetSpeedClicked()
{
	scrollSensorSpeedNew.setVisible(true);
	Selected = 2;
	BTNConfirm.setVisible(true);
	BTNCancel.setVisible(true);
	BTNWrite.setVisible(false);
	scrollSensorSpeedNew.invalidate();
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNWrite.invalidate();
}

//Кнопка подтверждения выбранного значения
void Settings5View::BTNConfirmClicked()
{
	switch (Selected)
	{
	case 2:
		Unicode::snprintf(BTNSetSpeedBuffer, BTNSETSPEED_SIZE, "%d", SetSpeed);
		scrollSensorSpeedNew.setVisible(false);
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
	scrollSensorSpeedNew.setVisible(false);
	BTNConfirm.setVisible(false);
	BTNCancel.setVisible(false);
	BTNWrite.setVisible(true);
	BTNConfirm.invalidate();
	BTNCancel.invalidate();
	BTNWrite.invalidate();
	Settings5ViewBase::setupScreen();
}
