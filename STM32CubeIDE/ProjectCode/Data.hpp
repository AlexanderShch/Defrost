/*
 * Data.hpp
 *
 *  Created on: Jul 3, 2023
 *      Author: gdr
 */

#ifndef DATA_HPP_
#define DATA_HPP_

#include "cmsis_os.h"
#include "ModBus.hpp"

#define TQ 16				// time quantity for saving measures in array
#define SQ 7				// датчики TH дефростера (0-2) + датчики продукта (3, 4) + T корпуса (5) + MB_IO (6)
#define STQ 5				// sensors type quantity - кол-во типов датчиков и модулей IO
#define FLAG_ReadData 1ul	// read data event flag 0x00000001ul
// Интервал отправки телеметрии на сервер по умолчанию (сек).
// Почему: 10 секунд — безопасная нагрузка на линию/сервер и ожидаемое значение по умолчанию.
#define TELEMETRY_INTERVAL_DEFAULT_SEC 10u

class Sensor
{
public:
	Sensor(){};										// declare default constructor
	Sensor(unsigned int Time, int T, int H){};		// declare constructor
	static void PutData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param, int Val);
	static int GetData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param);

protected:
	static unsigned int Time[TQ][SQ];	// number of time quantum measuring
	static int T[TQ][SQ];			// temperature
	static int H[TQ][SQ];			// humidity
};

// Функция повторной отправки последних данных телеметрии
void ResendLastTelemetry(void);

// Текущий интервал отправки телеметрии (сек). Изменяется командой CFG_CMD_SET_INTERVAL.
extern volatile uint16_t g_TelemetryIntervalSeconds;

// Установить новый интервал отправки телеметрии (сек).
// Почему: интервал меняется из обработчика команд и должен применяться сразу (сброс счётчика).
void Telemetry_SetIntervalSeconds(uint16_t intervalSeconds);

#endif /* DATA_HPP_ */



