#include <gui/settings1_screen/Settings1View.hpp>
#include <touchgfx/utils.hpp>
#include <iostream>

extern SENSOR_typedef_t Sensor_array[SQ];

namespace
{
	bool isToggleOn(const touchgfx::ToggleButton& button)
	{
		// Почему: для этих кнопок используем прямое соответствие: 0 = не использовать, 1 = использовать.
		return button.getState() != 0;
	}

	void setToggleOn(touchgfx::ToggleButton& button, bool on)
	{
		// Почему: состояние UI должно совпадать со смыслом: 0 = не использовать, 1 = использовать.
		button.forceState(on);
		button.invalidate();
	}

	int16_t findSensorIndexByAddress(uint8_t address)
	{
		for (int16_t i = 0; i < SQ; i++)
		{
			if (Sensor_array[i].Address == address)
			{
				return i;
			}
		}
		return -1;
	}

	constexpr uint8_t kCoreTLeftSensorAddress = 104;  // "Left prod"
	constexpr uint8_t kCoreTRightSensorAddress = 105; // "Right prod"
}

Settings1View::Settings1View()
	: coreTSensorToggleCallback(this, &Settings1View::coreTSensorToggleCallbackHandler)
{
}

void Settings1View::setupScreen()
{
    // Обновляем значение на экране в соответствии с текущей уставкой CoreTSet.
    Settings1ViewBase::setupScreen();

	BTNCoreTSensorLeft.setAction(coreTSensorToggleCallback);
	BTNCoreTSensorRight.setAction(coreTSensorToggleCallback);

	const int16_t leftIdx = findSensorIndexByAddress(kCoreTLeftSensorAddress);
	const int16_t rightIdx = findSensorIndexByAddress(kCoreTRightSensorAddress);

	if (leftIdx >= 0)
	{
		setToggleOn(BTNCoreTSensorLeft, Sensor_array[leftIdx].UseInDefrost != 0);
	}
	if (rightIdx >= 0)
	{
		setToggleOn(BTNCoreTSensorRight, Sensor_array[rightIdx].UseInDefrost != 0);
	}

    Unicode::snprintf(ValueCoreTSetBuffer, VALUECORETSET_SIZE, "%d", CoreTSet);
	// Почему: задаём фиксированную широкую область, чтобы при переходе 2→1 цифра не оставляла артефактов
	// (инвалидация всегда перерисовывает один и тот же прямоугольник).
	ValueCoreTSet.setPosition(BoxCoreT.getX(), ValueCoreTSet.getY(), BoxCoreT.getWidth(), ValueCoreTSet.getHeight());
    ValueCoreTSet.invalidate();
}

void Settings1View::tearDownScreen()
{
    Settings1ViewBase::tearDownScreen();
}

void Settings1View::coreTSensorToggleCallbackHandler(const touchgfx::AbstractButton& src)
{
	if (&src == &BTNCoreTSensorLeft)
	{
		const int16_t leftIdx = findSensorIndexByAddress(kCoreTLeftSensorAddress);
		if (leftIdx >= 0)
		{
			Sensor_array[leftIdx].UseInDefrost = isToggleOn(BTNCoreTSensorLeft) ? 1 : 0;
		}
	}
	else if (&src == &BTNCoreTSensorRight)
	{
		const int16_t rightIdx = findSensorIndexByAddress(kCoreTRightSensorAddress);
		if (rightIdx >= 0)
		{
			Sensor_array[rightIdx].UseInDefrost = isToggleOn(BTNCoreTSensorRight) ? 1 : 0;
		}
	}
}

void Settings1View::BTNCoreTSetIncreaseClicked()
{
    // Увеличиваем уставку CoreTSet на 1 в допустимых пределах и обновляем отображение.
    if (CoreTSet <= 19)
    {
        CoreTSet++;
        Unicode::snprintf(ValueCoreTSetBuffer, VALUECORETSET_SIZE, "%d", CoreTSet);
        ValueCoreTSet.invalidate();
        presenter->DefrosterOperatingTemperaturePresenter(CoreTSet);
    }
}

void Settings1View::BTNCoreTSetDecreaseClicked()
{
    // Уменьшаем уставку CoreTSet на 1 в допустимых пределах и обновляем отображение.
    if (CoreTSet >= 1)
    {
        CoreTSet--;
        Unicode::snprintf(ValueCoreTSetBuffer, VALUECORETSET_SIZE, "%d", CoreTSet);
        ValueCoreTSet.invalidate();
        presenter->DefrosterOperatingTemperaturePresenter(CoreTSet);
    }
}

