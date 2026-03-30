#include <gui/air_out_screen/Air_OutView.hpp>
#include <gui/common/AirFlapState.hpp>
#include <gui/common/ExhaustOutputState.hpp>
#include <gui/model/Model.hpp>

namespace
{
	bool isToggleOn(const touchgfx::ToggleButton& button)
	{
		// Для BTN_Manual: Pressed = ON, Released = OFF.
		return button.getState() != 0;
	}

	void setToggleOn(touchgfx::ToggleButton& button, bool on)
	{
		button.forceState(on);
		button.invalidate();
	}
}

Air_OutView::Air_OutView()
{

}

void Air_OutView::setupScreen()
{
    Air_OutViewBase::setupScreen();
	// Как в Settings2: на входе синхронизируем BTN_Manual с текущим режимом.
	setToggleOn(BTN_Manual, Model::isDefrostManualModeEnabled());
	syncExhaustFanAndFlapFromInputs();
}

void Air_OutView::handleTickEvent()
{
	Air_OutViewBase::handleTickEvent();
	syncExhaustFanAndFlapFromInputs();
}

void Air_OutView::syncExhaustFanAndFlapFromInputs()
{
	static uint8_t s_airFanLastInterval = 0u;

	if (IsExhaustFanOn((unsigned char)Model::DFR_current._Out))
	{
		if (!anim_AirFan.isAnimatedImageRunning())
		{
			anim_AirFan.startAnimation(false, false, true);
		}
		const uint8_t wantedInterval = 3u;
		if (s_airFanLastInterval != wantedInterval)
		{
			anim_AirFan.setUpdateTicksInterval(wantedInterval);
			s_airFanLastInterval = wantedInterval;
		}
	}
	else
	{
		if (anim_AirFan.isAnimatedImageRunning())
		{
			anim_AirFan.pauseAnimation();
		}
		s_airFanLastInterval = 0u;
	}
	anim_AirFan.invalidate();

	// Отображение положения заслонки по входным сигналам DI.
	const AirFlapState flapState = ResolveAirFlapState(
		(unsigned char)Model::DI_DFR.Bits.Air_Open,
		(unsigned char)Model::DI_DFR.Bits.Air_Close);

	const bool showFlapOn = (flapState == AirFlapState::Open);
	const bool showFlapOff = (flapState == AirFlapState::Closed);
	const bool showFlapMoving = (flapState == AirFlapState::Moving);

	Flap_On.setVisible(showFlapOn);
	Flap_Off.setVisible(showFlapOff);
	Flap.setVisible(showFlapMoving);
	Flap_On.invalidate();
	Flap_Off.invalidate();
	Flap.invalidate();
}

void Air_OutView::tearDownScreen()
{
    Air_OutViewBase::tearDownScreen();
}

void Air_OutView::BTNManualClicked()
{
	// По клику меняем режим дефростера (ручной/автоматический) согласно состоянию кнопки.
	const bool manualEnabled = isToggleOn(BTN_Manual);
	Model::setDefrostManualModeEnabled(manualEnabled);
}
