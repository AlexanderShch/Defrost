#include <gui/settings3_screen/Settings3View.hpp>
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

Settings3View::Settings3View()
{

}

void Settings3View::setupScreen()
{
    Settings3ViewBase::setupScreen();

	const bool group1Enabled = Model::isDefrostManualGroupEnabled(1);
	const bool group2Enabled = Model::isDefrostManualGroupEnabled(2);
	const bool fan1Enabled = (Model::DFR_manual.Vent1_Left != 0);
	const bool fan2Enabled = (Model::DFR_manual.Vent2_Left != 0);
	const bool fans12Enabled = fan1Enabled && fan2Enabled;
	const bool heat1Enabled = fans12Enabled && (Model::DFR_manual.Ten1_Left != 0);
	const bool heat2Enabled = fans12Enabled && (Model::DFR_manual.Ten2_Left != 0);

	// Почему: чтобы UI соответствовал сохранённому состоянию ручного режима при переходах между экранами.
	setToggleOn(BTNManualControl1, group1Enabled);
	setToggleOn(BTNManualControl2, group2Enabled);

	setVisible(BTNFanControl1, group1Enabled);
	setVisible(BTNFanControl2, group2Enabled);
	setToggleOn(BTNFanControl1, fan1Enabled);
	setToggleOn(BTNFanControl2, fan2Enabled);

	// Почему: нагрев разрешён только когда оба вентилятора блока включены.
	setVisible(BTNHeatControl1, group1Enabled && fans12Enabled);
	setVisible(BTNHeatControl2, group2Enabled && fans12Enabled);
	setToggleOn(BTNHeatControl1, heat1Enabled);
	setToggleOn(BTNHeatControl2, heat2Enabled);

	if (!group1Enabled)
	{
		forceOff(BTNFanControl1);
		forceOff(BTNHeatControl1);
		forceOff(BTNHeatControl2);
		setVisible(BTNHeatControl1, false);
		setVisible(BTNHeatControl2, false);
		Model::DFR_manual.Vent1_Left = 0;
		Model::DFR_manual.Ten1_Left = 0;
		Model::DFR_manual.Ten2_Left = 0;
	}
	else if (!fan1Enabled)
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten1_Left = 0;
		Model::DFR_manual.Ten2_Left = 0;
		forceOff(BTNHeatControl1);
		forceOff(BTNHeatControl2);
		setVisible(BTNHeatControl1, false);
		setVisible(BTNHeatControl2, false);
	}
	if (!group2Enabled)
	{
		forceOff(BTNFanControl2);
		forceOff(BTNHeatControl2);
		forceOff(BTNHeatControl1);
		setVisible(BTNHeatControl1, false);
		setVisible(BTNHeatControl2, false);
		Model::DFR_manual.Vent2_Left = 0;
		Model::DFR_manual.Ten1_Left = 0;
		Model::DFR_manual.Ten2_Left = 0;
	}
	else if (!fan2Enabled)
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten2_Left = 0;
		Model::DFR_manual.Ten1_Left = 0;
		forceOff(BTNHeatControl2);
		forceOff(BTNHeatControl1);
		setVisible(BTNHeatControl2, false);
		setVisible(BTNHeatControl1, false);
	}
}

void Settings3View::tearDownScreen()
{
    Settings3ViewBase::tearDownScreen();
}

void Settings3View::BTNManual1Clicked()
{
	const bool groupEnabled = isToggleOn(BTNManualControl1);
	Model::setDefrostManualGroupEnabled(1, groupEnabled);

	if (groupEnabled)
	{
		setVisible(BTNFanControl1, true);

		const bool fans12Enabled = (Model::DFR_manual.Vent1_Left != 0) && (Model::DFR_manual.Vent2_Left != 0);
		setVisible(BTNHeatControl1, fans12Enabled);
		setVisible(BTNHeatControl2, fans12Enabled);
		if (!fans12Enabled)
		{
			Model::DFR_manual.Ten1_Left = 0;
			Model::DFR_manual.Ten2_Left = 0;
			forceOff(BTNHeatControl1);
			forceOff(BTNHeatControl2);
		}
	}
	else
	{
		setVisible(BTNFanControl1, false);
		setVisible(BTNHeatControl1, false);
		setVisible(BTNHeatControl2, false);
		forceOff(BTNFanControl1);
		forceOff(BTNHeatControl1);
		forceOff(BTNHeatControl2);

		// Почему: группа выключена, значит её выходы не должны оставаться "залипшими" в регистре ручного управления.
		Model::DFR_manual.Vent1_Left = 0;
		Model::DFR_manual.Ten1_Left = 0;
		Model::DFR_manual.Ten2_Left = 0;
	}
}

void Settings3View::BTNManual2Clicked()
{
	const bool groupEnabled = isToggleOn(BTNManualControl2);
	Model::setDefrostManualGroupEnabled(2, groupEnabled);

	if (groupEnabled)
	{
		setVisible(BTNFanControl2, true);

		const bool fans12Enabled = (Model::DFR_manual.Vent1_Left != 0) && (Model::DFR_manual.Vent2_Left != 0);
		setVisible(BTNHeatControl1, fans12Enabled);
		setVisible(BTNHeatControl2, fans12Enabled);
		if (!fans12Enabled)
		{
			Model::DFR_manual.Ten2_Left = 0;
			Model::DFR_manual.Ten1_Left = 0;
			forceOff(BTNHeatControl2);
			forceOff(BTNHeatControl1);
		}
	}
	else
	{
		setVisible(BTNFanControl2, false);
		setVisible(BTNHeatControl2, false);
		setVisible(BTNHeatControl1, false);
		forceOff(BTNFanControl2);
		forceOff(BTNHeatControl2);
		forceOff(BTNHeatControl1);

		// Почему: группа выключена, значит её выходы не должны оставаться "залипшими" в регистре ручного управления.
		Model::DFR_manual.Vent2_Left = 0;
		Model::DFR_manual.Ten1_Left = 0;
		Model::DFR_manual.Ten2_Left = 0;
	}
}

void Settings3View::BTNFanControl1Clicked()
{
	if (!isToggleOn(BTNManualControl1))
	{
		return;
	}

	const bool fanEnabled = isToggleOn(BTNFanControl1);
	Model::DFR_manual.Vent1_Left = fanEnabled ? 1 : 0;

	const bool fans12Enabled = (Model::DFR_manual.Vent1_Left != 0) && (Model::DFR_manual.Vent2_Left != 0);
	if (fans12Enabled)
	{
		setVisible(BTNHeatControl1, true);
		setVisible(BTNHeatControl2, true);
	}
	else
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten1_Left = 0;
		Model::DFR_manual.Ten2_Left = 0;
		forceOff(BTNHeatControl1);
		forceOff(BTNHeatControl2);
		setVisible(BTNHeatControl1, false);
		setVisible(BTNHeatControl2, false);
	}
}

void Settings3View::BTNFanControl2Clicked()
{
	if (!isToggleOn(BTNManualControl2))
	{
		return;
	}

	const bool fanEnabled = isToggleOn(BTNFanControl2);
	Model::DFR_manual.Vent2_Left = fanEnabled ? 1 : 0;

	const bool fans12Enabled = (Model::DFR_manual.Vent1_Left != 0) && (Model::DFR_manual.Vent2_Left != 0);
	if (fans12Enabled)
	{
		setVisible(BTNHeatControl1, true);
		setVisible(BTNHeatControl2, true);
	}
	else
	{
		// Почему: нагрев не должен быть включён, если оба вентилятора не включены.
		Model::DFR_manual.Ten2_Left = 0;
		Model::DFR_manual.Ten1_Left = 0;
		forceOff(BTNHeatControl2);
		forceOff(BTNHeatControl1);
		setVisible(BTNHeatControl2, false);
		setVisible(BTNHeatControl1, false);
	}
}

void Settings3View::BTNHeatControl1Clicked()
{
	if (!isToggleOn(BTNManualControl1))
	{
		return;
	}

	if (Model::DFR_manual.Vent1_Left == 0 || Model::DFR_manual.Vent2_Left == 0)
	{
		// Почему: поддерживаем инвариант, даже если UI стал несогласованным из-за пропущенных событий.
		Model::DFR_manual.Ten1_Left = 0;
		forceOff(BTNHeatControl1);
		return;
	}

	Model::DFR_manual.Ten1_Left = isToggleOn(BTNHeatControl1) ? 1 : 0;
}

void Settings3View::BTNHeatControl2Clicked()
{
	if (!isToggleOn(BTNManualControl2))
	{
		return;
	}

	if (Model::DFR_manual.Vent1_Left == 0 || Model::DFR_manual.Vent2_Left == 0)
	{
		// Почему: поддерживаем инвариант, даже если UI стал несогласованным из-за пропущенных событий.
		Model::DFR_manual.Ten2_Left = 0;
		forceOff(BTNHeatControl2);
		return;
	}

	Model::DFR_manual.Ten2_Left = isToggleOn(BTNHeatControl2) ? 1 : 0;
}