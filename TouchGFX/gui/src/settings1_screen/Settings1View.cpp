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

	// Целевая мин. Т рыбы °C — из параметров алгоритма (синхронизация с ValueCoreTSet на экране).
	float t = presenter->GetFishColdTarget_C();
	// Храним уставку в десятых градуса, шаг = 0.1 °C.
	int vDeci = static_cast<int>(t * 10.0f + 0.5f);
	if (vDeci < 10)  vDeci = 10;   // 1.0 °C
	if (vDeci > 190) vDeci = 190;  // 19.0 °C
	CoreTSetDeci = static_cast<int16_t>(vDeci);

    applyCoreTSetAndRedraw();
	// Почему: задаём фиксированную широкую область, чтобы при переходе 2→1 цифра не оставляла артефактов
	// (инвалидация всегда перерисовывает один и тот же прямоугольник).
	ValueCoreTSet.setPosition(BoxCoreT.getX(), ValueCoreTSet.getY(), BoxCoreT.getWidth(), ValueCoreTSet.getHeight());
    ValueCoreTSet.invalidate();
}

void Settings1View::tearDownScreen()
{
    Settings1ViewBase::tearDownScreen();
}

void Settings1View::applyCoreTSetAndRedraw()
{
	// Отображаем одно десятичное: X.Y
	int whole = CoreTSetDeci / 10;
	int frac  = CoreTSetDeci >= 0 ? (CoreTSetDeci % 10) : -(CoreTSetDeci % 10);
	Unicode::snprintf(ValueCoreTSetBuffer, VALUECORETSET_SIZE, "%d.%d", whole, frac);
	ValueCoreTSet.invalidate();

	// Передаём уставку алгоритму в градусах Цельсия.
	float valC = static_cast<float>(CoreTSetDeci) / 10.0f;
	presenter->DefrosterOperatingTemperaturePresenter(valC);
}

void Settings1View::stepIncreaseOnce()
{
	if (CoreTSetDeci <= 190)
	{
		CoreTSetDeci += 1; // +0.1 °C
		if (CoreTSetDeci > 190)
			CoreTSetDeci = 190;
		applyCoreTSetAndRedraw();
	}
}

void Settings1View::stepDecreaseOnce()
{
	if (CoreTSetDeci >= 10)
	{
		CoreTSetDeci -= 1; // -0.1 °C
		if (CoreTSetDeci < 10)
			CoreTSetDeci = 10;
		applyCoreTSetAndRedraw();
	}
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
    // Однократное нажатие: шаг +0.1 °C.
	stepIncreaseOnce();
}

void Settings1View::BTNCoreTSetDecreaseClicked()
{
    // Однократное нажатие: шаг -0.1 °C.
	stepDecreaseOnce();
}

void Settings1View::handleTickEvent()
{
	Settings1ViewBase::handleTickEvent();

	// Предполагаем частоту тиков ~60 Гц:
	// 2 с удержания ≈ 120 тиков, повтор каждые 0.3 с ≈ 18 тиков.
	const uint16_t longPressTicks = 120;
	const uint16_t repeatIntervalTicks = 18;

	// Кнопка увеличения.
	bool incNow = BTNCoreTSetIncrease.getPressedState();
	if (incNow)
	{
		if (!incPressedPrev)
		{
			incPressTicks = 0;
			incRepeatTicks = 0;
		}
		else
		{
			incPressTicks++;
			if (incPressTicks >= longPressTicks)
			{
				if (incRepeatTicks >= repeatIntervalTicks)
				{
					incRepeatTicks = 0;
					stepIncreaseOnce();
				}
				else
				{
					incRepeatTicks++;
				}
			}
		}
	}
	else
	{
		incPressTicks = 0;
		incRepeatTicks = 0;
	}
	incPressedPrev = incNow;

	// Кнопка уменьшения.
	bool decNow = BTNCoreTSetDecrease.getPressedState();
	if (decNow)
	{
		if (!decPressedPrev)
		{
			decPressTicks = 0;
			decRepeatTicks = 0;
		}
		else
		{
			decPressTicks++;
			if (decPressTicks >= longPressTicks)
			{
				if (decRepeatTicks >= repeatIntervalTicks)
				{
					decRepeatTicks = 0;
					stepDecreaseOnce();
				}
				else
				{
					decRepeatTicks++;
				}
			}
		}
	}
	else
	{
		decPressTicks = 0;
		decRepeatTicks = 0;
	}
	decPressedPrev = decNow;
}

