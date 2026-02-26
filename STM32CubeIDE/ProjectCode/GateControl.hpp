#ifndef GATECONTROL_HPP_
#define GATECONTROL_HPP_

#include <stdint.h>

// Команда управления воротами через единый API.
enum class GateControlCommand : uint8_t
{
	Open = 0,
	Close = 1,
	Deblock = 2
};

// API управления воротами (используется и ручным, и автоматическим режимами).
void GateControl_SetManualMode(uint8_t enabled);
uint8_t GateControl_GetManualMode(void);
void GateControl_SetCommand(GateControlCommand command, uint8_t enabled);
uint8_t GateControl_IsCommandActive(GateControlCommand command);
uint8_t GateControl_IsOpenPosition(void);
uint8_t GateControl_IsClosedPosition(void);
uint8_t GateControl_IsAlarm(void);

// Обновление логики ворот с периодом 1 сек.
void GateControl_Update1s(void);

#endif /* GATECONTROL_HPP_ */
