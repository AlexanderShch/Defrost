#include "GateControl.hpp"

#include <gui/model/Model.hpp>

namespace
{
// Gate_Open/Gate_Close обновляются сразу после чтения DI в ModBus.cpp.
uint8_t gateUpElapsedSec = 0;
uint8_t gateDownElapsedSec = 0;
uint8_t prevCmdUp = 0;
uint8_t prevCmdDown = 0;
uint8_t prevGateOpen = 0;
uint8_t prevGateClose = 0;

DFR_REGISTERS_t& GateRegisterByCurrentMode()
{
	// Почему: на реле уходит только активный регистр выбранного режима.
	// Команды ворот должны писаться в тот же регистр, чтобы выполняться без принудительного переключения режима.
	return (Model::Flag_DFR_manual != 0) ? Model::DFR_manual : Model::DFR;
}
}

void GateControl_SetManualMode(uint8_t enabled)
{
	Model::Flag_DFR_manual = enabled ? 1 : 0;
}

uint8_t GateControl_GetManualMode(void)
{
	return Model::Flag_DFR_manual ? 1 : 0;
}

void GateControl_SetCommand(GateControlCommand command, uint8_t enabled)
{
	const bool en = (enabled != 0);
	DFR_REGISTERS_t& gateRegister = GateRegisterByCurrentMode();

	switch (command)
	{
	case GateControlCommand::Open:
		// При старте открытия в аварии снимаем фиксацию нижнего положения.
		if (en && Model::Gate_Alarm != 0)
		{
			Model::Gate_PosBottom = 0;
		}
		gateRegister.Gate_Up = en ? 1 : 0;
		if (en)
		{
			gateRegister.Gate_Down = 0;
			if (GateControl_GetManualMode() == 0)
				gateRegister.Gate_Dbl = 0;
		}
		break;

	case GateControlCommand::Close:
		// При старте закрытия в аварии снимаем фиксацию верхнего положения.
		if (en && Model::Gate_Alarm != 0)
		{
			Model::Gate_PosTop = 0;
		}
		gateRegister.Gate_Down = en ? 1 : 0;
		if (en)
		{
			gateRegister.Gate_Up = 0;
			if (GateControl_GetManualMode() == 0)
				gateRegister.Gate_Dbl = 0;
		}
		break;

	case GateControlCommand::Deblock:
		gateRegister.Gate_Dbl = en ? 1 : 0;
		if (en)
		{
			gateRegister.Gate_Up = 0;
			gateRegister.Gate_Down = 0;
		}
		break;

	default:
		break;
	}
}

uint8_t GateControl_IsCommandActive(GateControlCommand command)
{
	const DFR_REGISTERS_t& gateRegister = GateRegisterByCurrentMode();

	switch (command)
	{
	case GateControlCommand::Open:
		return (gateRegister.Gate_Up != 0) ? 1 : 0;
	case GateControlCommand::Close:
		return (gateRegister.Gate_Down != 0) ? 1 : 0;
	case GateControlCommand::Deblock:
		return (gateRegister.Gate_Dbl != 0) ? 1 : 0;
	default:
		return 0;
	}
}

uint8_t GateControl_IsOpenPosition(void)
{
	if (Model::Gate_Alarm != 0)
	{
		return (Model::Gate_PosTop != 0) ? 1 : 0;
	}
	return (Model::DI_DFR.Bits.Gate_Open != 0) ? 1 : 0;
}

uint8_t GateControl_IsClosedPosition(void)
{
	if (Model::Gate_Alarm != 0)
	{
		return (Model::Gate_PosBottom != 0) ? 1 : 0;
	}
	return (Model::DI_DFR.Bits.Gate_Close != 0) ? 1 : 0;
}

uint8_t GateControl_IsAlarm(void)
{
	return (Model::Gate_Alarm != 0) ? 1 : 0;
}

void GateControl_Update1s(void)
{
	DFR_REGISTERS_t& gateRegister = GateRegisterByCurrentMode();
	const bool cmdUp = (gateRegister.Gate_Up != 0);
	const bool cmdDown = (gateRegister.Gate_Down != 0);

	// Почему: сигнал Gate_Open/Gate_Close может быть "залипшим" в 1.
	// Завершение движения считаем только по фронту 0->1 после начала движения.
	const uint8_t gateOpen = (Model::DI_DFR.Bits.Gate_Open != 0) ? 1 : 0;
	const uint8_t gateClose = (Model::DI_DFR.Bits.Gate_Close != 0) ? 1 : 0;
	const bool gateOpenEdge = (gateOpen != 0) && (prevGateOpen == 0);
	const bool gateCloseEdge = (gateClose != 0) && (prevGateClose == 0);

	// Начало движения в противоположном направлении сбрасывает флаг конечного положения в аварийном режиме.
	if (cmdUp && !prevCmdUp)
	{
		Model::Gate_PosBottom = 0;
	}
	if (cmdDown && !prevCmdDown)
	{
		Model::Gate_PosTop = 0;
	}
	prevCmdUp = cmdUp ? 1 : 0;
	prevCmdDown = cmdDown ? 1 : 0;

	// Подъём ворот.
	if (cmdUp)
	{
		if (gateOpenEdge)
		{
			gateRegister.Gate_Up = 0;
			gateUpElapsedSec = 0;
			Model::Gate_Alarm_Program = 0;
			Model::Gate_Alarm = ((Model::Gate_Alarm_Program != 0u) || (Model::Gate_Alarm_Hardware != 0u)) ? 1u : 0u;
			Model::Gate_PosTop = 1;
			Model::Gate_PosBottom = 0;
		}
		else
		{
			if (gateUpElapsedSec < 10)
			{
				++gateUpElapsedSec;
			}
			if (gateUpElapsedSec >= 10)
			{
				gateRegister.Gate_Up = 0;
				gateUpElapsedSec = 0;
				Model::Gate_Alarm_Program = 1;
				Model::Gate_Alarm = 1;
				Model::Gate_PosTop = 1;
				Model::Gate_PosBottom = 0;
			}
		}
	}
	else
	{
		gateUpElapsedSec = 0;
	}

	// Опускание ворот.
	if (cmdDown)
	{
		if (gateCloseEdge)
		{
			gateRegister.Gate_Down = 0;
			gateDownElapsedSec = 0;
			Model::Gate_Alarm_Program = 0;
			Model::Gate_Alarm = ((Model::Gate_Alarm_Program != 0u) || (Model::Gate_Alarm_Hardware != 0u)) ? 1u : 0u;
			Model::Gate_PosBottom = 1;
			Model::Gate_PosTop = 0;
		}
		else
		{
			if (gateDownElapsedSec < 10)
			{
				++gateDownElapsedSec;
			}
			if (gateDownElapsedSec >= 10)
			{
				gateRegister.Gate_Down = 0;
				gateDownElapsedSec = 0;
				Model::Gate_Alarm_Program = 1;
				Model::Gate_Alarm = 1;
				Model::Gate_PosBottom = 1;
				Model::Gate_PosTop = 0;
			}
		}
	}
	else
	{
		gateDownElapsedSec = 0;
	}

	prevGateOpen = gateOpen;
	prevGateClose = gateClose;
}
