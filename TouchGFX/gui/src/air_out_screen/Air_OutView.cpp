#include <gui/air_out_screen/Air_OutView.hpp>
#include <gui/common/AirFlapState.hpp>
#include <gui/common/ExhaustOutputState.hpp>
#include <gui/model/Model.hpp>

#include <stdint.h>

namespace
{
	const uint8_t MinAlpha = 50;

	bool isToggleOn(const touchgfx::ToggleButton& button)
	{
		// Pressed = ON, Released = OFF.
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

	// Инвариант: без команды открытия заслонки вентилятор вручную не включаем.
	if (Model::DFR_manual.Water_Flap == 0)
	{
		Model::DFR_manual._Out = 0;
	}
	setToggleOn(BTN_AirFlap, Model::DFR_manual.Water_Flap != 0);
	setToggleOn(BTN_AirFan, Model::DFR_manual._Out != 0);
	updateAirOutManualControls();
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
	const uint16_t kOutFanAlarmBit = (uint16_t)(1u << 8);   // рассогласование _Out (DO->DI)
	const uint16_t kFlapAlarmBit = (uint16_t)(1u << 11);    // таймаут перехода заслонки

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

	// Мигать аварийными надписями синхронно с красной лампой _Alr.
	const bool alarmBlinkOnPhase = (Model::DFR_current._Alr != 0u);
	const uint16_t deviceAlarmFlags = Model::Device_AlarmFlags;
	const bool flapAlarmActive = (deviceAlarmFlags & kFlapAlarmBit) != 0u;
	const bool outFanAlarmActive = (deviceAlarmFlags & kOutFanAlarmBit) != 0u;

	const bool showFlapAlarmLabel = flapAlarmActive && alarmBlinkOnPhase;
	const bool showFanAlarmLabel = outFanAlarmActive && alarmBlinkOnPhase;

	LabelFlapAlarm.setVisible(showFlapAlarmLabel);
	Label_AirFlap.setVisible(!showFlapAlarmLabel);
	LabelAirFanAlarm.setVisible(showFanAlarmLabel);
	Label_AirFan.setVisible(!showFanAlarmLabel);
	LabelFlapAlarm.invalidate();
	Label_AirFlap.invalidate();
	LabelAirFanAlarm.invalidate();
	Label_AirFan.invalidate();
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
	if (!manualEnabled)
	{
		// Model обнулила DFR_manual; сбрасываем переключатели на экране.
		setToggleOn(BTN_AirFlap, false);
		setToggleOn(BTN_AirFan, false);
	}
	updateAirOutManualControls();
}

void Air_OutView::updateAirOutManualControls()
{
	const bool manual = Model::isDefrostManualModeEnabled();
	const bool flapOn = isToggleOn(BTN_AirFlap);

	if (!manual)
	{
		BTN_AirFlap.setTouchable(false);
		BTN_AirFlap.setAlpha(MinAlpha);
		BTN_AirFan.setTouchable(false);
		BTN_AirFan.setAlpha(MinAlpha);
		Label_AirFlap.setAlpha(MinAlpha);
		Label_AirFan.setAlpha(MinAlpha);
	}
	else
	{
		BTN_AirFlap.setTouchable(true);
		BTN_AirFlap.setAlpha(255);
		Label_AirFlap.setAlpha(255);

		const bool fanEnabled = flapOn;
		BTN_AirFan.setTouchable(fanEnabled);
		BTN_AirFan.setAlpha(fanEnabled ? 255 : MinAlpha);
		Label_AirFan.setAlpha(fanEnabled ? 255 : MinAlpha);
	}

	BTN_AirFlap.invalidate();
	BTN_AirFan.invalidate();
	Label_AirFlap.invalidate();
	Label_AirFan.invalidate();
}

void Air_OutView::BTN_AirFlapClicked()
{
	if (!Model::isDefrostManualModeEnabled())
	{
		return;
	}

	const bool flapOn = isToggleOn(BTN_AirFlap);
	Model::DFR_manual.Water_Flap = flapOn ? 1 : 0;
	if (!flapOn)
	{
		Model::DFR_manual._Out = 0;
		setToggleOn(BTN_AirFan, false);
	}
	updateAirOutManualControls();
}

void Air_OutView::BTN_AirFanClicked()
{
	if (!Model::isDefrostManualModeEnabled())
	{
		return;
	}
	if (Model::DFR_manual.Water_Flap == 0)
	{
		return;
	}

	Model::DFR_manual._Out = isToggleOn(BTN_AirFan) ? 1 : 0;
}
