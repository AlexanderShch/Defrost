#include <gui/settings4_screen/Settings4View.hpp>
#include <gui/model/Model.hpp>
#include <touchgfx/utils.hpp>
#include <iostream>

namespace
{
	bool isToggleOn(const touchgfx::ToggleButton& button)
	{
		// Почему: в проекте перепутаны визуальные состояния ON/OFF (Pressed показывает OFF, Released показывает ON).
		return button.getState() == 0;
	}

	void setVisible(touchgfx::Drawable& widget, bool visible)
	{
		widget.setVisible(visible);
		widget.invalidate();
	}

	void forceOff(touchgfx::ToggleButton& button)
	{
		button.forceState(true);
		button.invalidate();
	}

	void setToggleOn(touchgfx::ToggleButton& button, bool on)
	{
		// Почему: чтобы соответствие UI-состояния и смысла ON/OFF было задано в одном месте.
		button.forceState(!on);
		button.invalidate();
	}
}

Settings4View::Settings4View()
{

}

void Settings4View::setupScreen()
{
    Settings4ViewBase::setupScreen();

	const bool group3Enabled = Model::isDefrostManualGroupEnabled(3);
	const bool group4Enabled = Model::isDefrostManualGroupEnabled(4);
	const bool fan3Enabled = (Model::DFR_manual.Vent1_Right != 0);
	const bool fan4Enabled = (Model::DFR_manual.Vent2_Right != 0);
	const bool fans34Enabled = fan3Enabled && fan4Enabled;
	const bool heat3Enabled = fans34Enabled && (Model::DFR_manual.Ten1_Right != 0);
	const bool heat4Enabled = fans34Enabled && (Model::DFR_manual.Ten2_Right != 0);

	// Почему: чтобы UI соответствовал сохранённому состоянию ручного режима при переходах между экранами.
	setToggleOn(BTNManualControl3, group3Enabled);
	setToggleOn(BTNManualControl4, group4Enabled);

	setVisible(BTNFanControl3, group3Enabled);
	setVisible(BTNFanControl4, group4Enabled);
	setToggleOn(BTNFanControl3, fan3Enabled);
	setToggleOn(BTNFanControl4, fan4Enabled);

	// Почему: нагрев разрешён только когда оба вентилятора блока включены.
	setVisible(BTNHeatControl3, group3Enabled && fans34Enabled);
	setVisible(BTNHeatControl4, group4Enabled && fans34Enabled);
	setToggleOn(BTNHeatControl3, heat3Enabled);
	setToggleOn(BTNHeatControl4, heat4Enabled);

	if (!group3Enabled)
	{
		forceOff(BTNFanControl3);
		forceOff(BTNHeatControl3);
		forceOff(BTNHeatControl4);
		setVisible(BTNHeatControl3, false);
		setVisible(BTNHeatControl4, false);
		Model::DFR_manual.Vent1_Right = 0;
		Model::DFR_manual.Ten1_Right = 0;
		Model::DFR_manual.Ten2_Right = 0;
	}
	else if (!fan3Enabled)
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten1_Right = 0;
		Model::DFR_manual.Ten2_Right = 0;
		forceOff(BTNHeatControl3);
		forceOff(BTNHeatControl4);
		setVisible(BTNHeatControl3, false);
		setVisible(BTNHeatControl4, false);
	}
	if (!group4Enabled)
	{
		forceOff(BTNFanControl4);
		forceOff(BTNHeatControl4);
		forceOff(BTNHeatControl3);
		setVisible(BTNHeatControl3, false);
		setVisible(BTNHeatControl4, false);
		Model::DFR_manual.Vent2_Right = 0;
		Model::DFR_manual.Ten1_Right = 0;
		Model::DFR_manual.Ten2_Right = 0;
	}
	else if (!fan4Enabled)
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten2_Right = 0;
		Model::DFR_manual.Ten1_Right = 0;
		forceOff(BTNHeatControl4);
		forceOff(BTNHeatControl3);
		setVisible(BTNHeatControl4, false);
		setVisible(BTNHeatControl3, false);
	}
}

void Settings4View::tearDownScreen()
{
    Settings4ViewBase::tearDownScreen();
}

void Settings4View::BTNManual3Clicked()
{
	const bool groupEnabled = isToggleOn(BTNManualControl3);
	Model::setDefrostManualGroupEnabled(3, groupEnabled);

	if (groupEnabled)
	{
		setVisible(BTNFanControl3, true);

		const bool fans34Enabled = (Model::DFR_manual.Vent1_Right != 0) && (Model::DFR_manual.Vent2_Right != 0);
		setVisible(BTNHeatControl3, fans34Enabled);
		setVisible(BTNHeatControl4, fans34Enabled);
		if (!fans34Enabled)
		{
			Model::DFR_manual.Ten1_Right = 0;
			Model::DFR_manual.Ten2_Right = 0;
			forceOff(BTNHeatControl3);
			forceOff(BTNHeatControl4);
		}
	}
	else
	{
		setVisible(BTNFanControl3, false);
		setVisible(BTNHeatControl3, false);
		setVisible(BTNHeatControl4, false);
		forceOff(BTNFanControl3);
		forceOff(BTNHeatControl3);
		forceOff(BTNHeatControl4);

		// Почему: группа выключена, значит её выходы не должны оставаться "залипшими" в регистре ручного управления.
		Model::DFR_manual.Vent1_Right = 0;
		Model::DFR_manual.Ten1_Right = 0;
		Model::DFR_manual.Ten2_Right = 0;
	}
}

void Settings4View::BTNManual4Clicked()
{
	const bool groupEnabled = isToggleOn(BTNManualControl4);
	Model::setDefrostManualGroupEnabled(4, groupEnabled);

	if (groupEnabled)
	{
		setVisible(BTNFanControl4, true);

		const bool fans34Enabled = (Model::DFR_manual.Vent1_Right != 0) && (Model::DFR_manual.Vent2_Right != 0);
		setVisible(BTNHeatControl3, fans34Enabled);
		setVisible(BTNHeatControl4, fans34Enabled);
		if (!fans34Enabled)
		{
			Model::DFR_manual.Ten2_Right = 0;
			Model::DFR_manual.Ten1_Right = 0;
			forceOff(BTNHeatControl4);
			forceOff(BTNHeatControl3);
		}
	}
	else
	{
		setVisible(BTNFanControl4, false);
		setVisible(BTNHeatControl4, false);
		setVisible(BTNHeatControl3, false);
		forceOff(BTNFanControl4);
		forceOff(BTNHeatControl4);
		forceOff(BTNHeatControl3);

		// Почему: группа выключена, значит её выходы не должны оставаться "залипшими" в регистре ручного управления.
		Model::DFR_manual.Vent2_Right = 0;
		Model::DFR_manual.Ten1_Right = 0;
		Model::DFR_manual.Ten2_Right = 0;
	}
}

void Settings4View::BTNFanControl3Clicked()
{
	if (!isToggleOn(BTNManualControl3))
	{
		return;
	}

	const bool fanEnabled = isToggleOn(BTNFanControl3);
	Model::DFR_manual.Vent1_Right = fanEnabled ? 1 : 0;

	const bool fans34Enabled = (Model::DFR_manual.Vent1_Right != 0) && (Model::DFR_manual.Vent2_Right != 0);
	if (fans34Enabled)
	{
		setVisible(BTNHeatControl3, true);
		setVisible(BTNHeatControl4, true);
	}
	else
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten1_Right = 0;
		Model::DFR_manual.Ten2_Right = 0;
		forceOff(BTNHeatControl3);
		forceOff(BTNHeatControl4);
		setVisible(BTNHeatControl3, false);
		setVisible(BTNHeatControl4, false);
	}
}

void Settings4View::BTNFanControl4Clicked()
{
	if (!isToggleOn(BTNManualControl4))
	{
		return;
	}

	const bool fanEnabled = isToggleOn(BTNFanControl4);
	Model::DFR_manual.Vent2_Right = fanEnabled ? 1 : 0;

	const bool fans34Enabled = (Model::DFR_manual.Vent1_Right != 0) && (Model::DFR_manual.Vent2_Right != 0);
	if (fans34Enabled)
	{
		setVisible(BTNHeatControl3, true);
		setVisible(BTNHeatControl4, true);
	}
	else
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten2_Right = 0;
		Model::DFR_manual.Ten1_Right = 0;
		forceOff(BTNHeatControl4);
		forceOff(BTNHeatControl3);
		setVisible(BTNHeatControl4, false);
		setVisible(BTNHeatControl3, false);
	}
}

void Settings4View::BTNHeatControl3Clicked()
{
	if (!isToggleOn(BTNManualControl3))
	{
		return;
	}

	if (Model::DFR_manual.Vent1_Right == 0 || Model::DFR_manual.Vent2_Right == 0)
	{
		// Почему: поддерживаем инвариант, даже если UI стал несогласованным из-за пропущенных событий.
		Model::DFR_manual.Ten1_Right = 0;
		forceOff(BTNHeatControl3);
		return;
	}

	Model::DFR_manual.Ten1_Right = isToggleOn(BTNHeatControl3) ? 1 : 0;
}

void Settings4View::BTNHeatControl4Clicked()
{
	if (!isToggleOn(BTNManualControl4))
	{
		return;
	}

	if (Model::DFR_manual.Vent1_Right == 0 || Model::DFR_manual.Vent2_Right == 0)
	{
		// Почему: поддерживаем инвариант, даже если UI стал несогласованным из-за пропущенных событий.
		Model::DFR_manual.Ten2_Right = 0;
		forceOff(BTNHeatControl4);
		return;
	}

	Model::DFR_manual.Ten2_Right = isToggleOn(BTNHeatControl4) ? 1 : 0;
}