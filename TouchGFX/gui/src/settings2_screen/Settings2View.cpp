#include <gui/settings2_screen/Settings2View.hpp>
#include <gui/model/Model.hpp>
#include "GateControl.hpp"

#include <touchgfx/Color.hpp>
#include <touchgfx/utils.hpp>
#include <stdint.h>
uint8_t MinAlpha = 50;

namespace
{
	bool isToggleOn(const touchgfx::ToggleButton& button)
	{
		// Почему: в проекте перепутаны визуальные состояния ON/OFF (Pressed показывает OFF, Released показывает ON).
		return button.getState() == 0;
	}

	void setToggleOn(touchgfx::ToggleButton& button, bool on)
	{
		// Почему: чтобы соответствие UI-состояния и смысла ON/OFF было задано в одном месте.
		button.forceState(!on);
		button.invalidate();
	}

	void setLatchedColor(touchgfx::ButtonWithLabel& button, bool latched, touchgfx::colortype latchedColor)
	{
		button.setLabelColor(latched ? latchedColor : touchgfx::Color::getColorFromRGB(255, 255, 255));
		button.invalidate();
	}
}


Settings2View::Settings2View()
{

}

void Settings2View::setupScreen()
{
    Settings2ViewBase::setupScreen();

	// Синхронизация положения переключателя с текущим режимом.
	setToggleOn(BTN_Manual, Model::isDefrostManualModeEnabled());

	// В ручном режиме элементы активны, в автоматическом — приглушены.
	Settings2View::SetAlpha(Model::isDefrostManualModeEnabled() ? 255 : MinAlpha);

	// Синхронизация положения форсунки с регистром ручного управления.
	setToggleOn(BTN_Spray, Model::DFR_manual._Inj != 0);

	UpdateGateIndicators();
}

void Settings2View::tearDownScreen()
{
    Settings2ViewBase::tearDownScreen();
}

void Settings2View::BTNManualClicked()
{
	const bool manualEnabled = isToggleOn(BTN_Manual);
	Model::setDefrostManualModeEnabled(manualEnabled);

	// Визуально включаем/выключаем элементы ручного управления на странице.
	Settings2View::SetAlpha(manualEnabled ? 255 : MinAlpha);

	UpdateGateIndicators();
}

void Settings2View::SetAlpha(uint8_t MinAlhpa)
{
	const bool enabled = (MinAlhpa == 255);

   	BTN_GateUP.setAlpha(MinAlhpa);
   	BTN_GateDOWN.setAlpha(MinAlhpa);
   	BTN_GateSTOP.setAlpha(MinAlhpa);
	BTN_Spray.setAlpha(MinAlhpa);
	LabelSprayOn.setAlpha(MinAlhpa);
	LabelSprayOff.setAlpha(MinAlhpa);

	// Почему: в "приглушённом" состоянии элементы должны быть неактивными для нажатия.
	BTN_GateUP.setTouchable(enabled);
	BTN_GateDOWN.setTouchable(enabled);
	BTN_GateSTOP.setTouchable(enabled);
	BTN_Spray.setTouchable(enabled);

	BTN_GateUP.invalidate();
	BTN_GateDOWN.invalidate();
	BTN_GateSTOP.invalidate();
	BTN_Spray.invalidate();
	LabelSprayOn.invalidate();
	LabelSprayOff.invalidate();
}

void Settings2View::BTNSprayClicked()
{
	if (!Model::isDefrostManualModeEnabled())
	{
		return;
	}

	// Форсунка воды: бит 9 (_Inj).
	Model::DFR_manual._Inj = isToggleOn(BTN_Spray) ? 1 : 0;
}

void Settings2View::UpdateGateIndicators()
{
	const auto white = touchgfx::Color::getColorFromRGB(255, 255, 255);
	const auto green = touchgfx::Color::getColorFromRGB(41, 227, 20);
	const auto red = touchgfx::Color::getColorFromRGB(227, 14, 14);

	// Состояние сигналов движения ворот/разблокировки (команды ручного управления)
	const bool cmdUp = (GateControl_IsCommandActive(GateControlCommand::Open) != 0);
	const bool cmdDown = (GateControl_IsCommandActive(GateControlCommand::Close) != 0);
	const bool cmdDbl = (GateControl_IsCommandActive(GateControlCommand::Deblock) != 0);

	const bool manualEnabled = Model::isDefrostManualModeEnabled();

	const bool gateAlarm = (GateControl_IsAlarm() != 0);

	const bool gateOpen = (GateControl_IsOpenPosition() != 0);
	const bool gateClose = (GateControl_IsClosedPosition() != 0);

	// Надпись режима ворот: при аварии показываем LabelGateAlarm, иначе — LabelGate.
	LabelGate.setVisible(!gateAlarm);
	LabelGateAlarm.setVisible(gateAlarm);
	LabelGate.invalidate();
	LabelGateAlarm.invalidate();

	// Блокировка кнопок по состоянию ворот:
	// - если ворота открыты, "ВВЕРХ" блокирована
	// - если ворота закрыты, "ВНИЗ" блокирована
	BTN_GateUP.setTouchable(manualEnabled && !gateOpen);
	BTN_GateDOWN.setTouchable(manualEnabled && !gateClose);
	BTN_GateSTOP.setTouchable(manualEnabled);

	// Почему: деактивированные кнопки должны выглядеть "дымчатыми",
	// как при выключенном ручном управлении.
	BTN_GateUP.setAlpha((manualEnabled && !gateOpen) ? 255 : MinAlpha);
	BTN_GateDOWN.setAlpha((manualEnabled && !gateClose) ? 255 : MinAlpha);
	BTN_GateUP.invalidate();
	BTN_GateDOWN.invalidate();

	if (gateAlarm)
	{
		// В аварии подсвечиваем зелёным активное движение до конца движения/тайм-аута.
		// Если движения нет, подсвечиваем красным зафиксированное конечное положение.
		const bool posTop = (Model::Gate_PosTop != 0);
		const bool posBottom = (Model::Gate_PosBottom != 0);
		BTN_GateUP.setLabelColor(cmdUp ? green : (posTop ? red : white));
		BTN_GateDOWN.setLabelColor(cmdDown ? green : (posBottom ? red : white));
		BTN_GateUP.invalidate();
		BTN_GateDOWN.invalidate();
	}
	else
	{
		// Почему: зелёный цвет нужен только пока команда активна.
		// Состояние Gate_Open/Gate_Close используется для блокировки кнопок, а не для подсветки.
		setLatchedColor(BTN_GateUP, cmdUp, green);
		setLatchedColor(BTN_GateDOWN, cmdDown, green);
	}

	setLatchedColor(BTN_GateSTOP, cmdDbl, red);

	// Дополнительно: если ручной режим выключен, визуально возвращаем базовый цвет.
	if (!manualEnabled)
	{
		BTN_GateUP.setLabelColor(white);
		BTN_GateDOWN.setLabelColor(white);
		BTN_GateSTOP.setLabelColor(white);
		BTN_GateUP.invalidate();
		BTN_GateDOWN.invalidate();
		BTN_GateSTOP.invalidate();
	}
}
void Settings2View::BTNGateUpClicked()
{
	if (!Model::isDefrostManualModeEnabled())
	{
		return;
	}

	const bool newState = (GateControl_IsCommandActive(GateControlCommand::Open) == 0);
	GateControl_SetCommand(GateControlCommand::Open, newState ? 1 : 0);
	UpdateGateIndicators();
}
void Settings2View::BTNGateStopClicked()
{
	if (!Model::isDefrostManualModeEnabled())
	{
		return;
	}

	const bool newState = (GateControl_IsCommandActive(GateControlCommand::Deblock) == 0);
	GateControl_SetCommand(GateControlCommand::Deblock, newState ? 1 : 0);
	UpdateGateIndicators();
}
void Settings2View::BTNGateDownClicked()
{
	if (!Model::isDefrostManualModeEnabled())
	{
		return;
	}

	const bool newState = (GateControl_IsCommandActive(GateControlCommand::Close) == 0);
	GateControl_SetCommand(GateControlCommand::Close, newState ? 1 : 0);
	UpdateGateIndicators();
}
