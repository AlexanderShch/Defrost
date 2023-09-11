/*
 * Data.c
 *
 *  Created on: Jul 3, 2023
 *      Author: gdr
 */
#include <Data.hpp>
#include "main.h"


#include <gui\model\model.hpp>

// ReadDataEventHandle was defined in main.c
extern osEventFlagsId_t ReadDataEventHandle;
extern SENSOR_typedef_t Sensor_array[SQ];
extern osThreadId_t TouchGFX_Task;
// MB_Master_Task_CPP() was defined in ModBus.cpp
//extern void MB_Master_Task_CPP();

uint32_t flags;				// flags for waiting event
int8_t SensorNumber;

// definition of static variable. Member function definitions belong in the scope where the class is defined.
// current number of measure
unsigned int TimeFromStart = 0;
unsigned int Sensor::Time[TQ][SQ] = {{0}};	// number of time quantum measuring
int Sensor::T[TQ][SQ] = {{0}};		// temperature
int Sensor::H[TQ][SQ] = {{0}};		// humidity


// Return integer value from measure array
// TimeFromStart - value of measure counter give correct data only for last TQ measures
// SensNum - number of interesting sensor
// Param - 1 for time, 2 for temperature, 3 for humidity
// Val - value of data
void Sensor::PutData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param, int Val) {
	int i = TimeFromStart % TQ;

	switch (Param)
	{
	case 1:
		Time[i][SensNum] = TimeFromStart;
		break;
	case 2:
		T[i][SensNum] = Val;
		break;
	case 3:
		H[i][SensNum] = Val;
		break;
	default:
		break;
	}
	;
}

//	 Return integer value from measure array
//	 TimeFromStart - value of measure counter give correct data only for last TQ measures
//	 SensNum - number of interesting sensor
//	 Param - 0 for active, 1 for time, 2 for temperature, 3 for humidity
int Sensor::GetData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param) {
	uint32_t i = TimeFromStart % TQ;
	switch (Param) {
	case 1:
		return Time[i][SensNum];
		break;
	case 2:
		return T[i][SensNum];
		break;
	case 3:
		return H[i][SensNum];
		break;
	default:
		return 0;
		break;
	}
}

// 1. Operating system timer 1 sec will start this function
void DataTimerFunc()
{
	// Здесь установка флага события для запуска задачи по считыванию данных
	osEventFlagsSet(ReadDataEventHandle, FLAG_ReadData);

	// моргнём светодиодом
	HAL_GPIO_TogglePin(GPIOG, LD4_Pin);
	osDelay(100);
	HAL_GPIO_TogglePin(GPIOG, LD4_Pin);

}

/* 2. The task ReadData reading data from sensors
 * 	0 - defroster T, H left
 * 	1 - defroster T, H right
 * 	2 - defroster T, H center
 *	3 - fish T left
 *	4 - fish T right
 * 	5 - defroster operating T
 * 	6 - product final T
*/
void ReadDataFunc() {
	int TempOld, HumOld = 0;
	int TempNew, HumNew = 0;

	// Инициализация датчиков при запуске задачи
	MB_Master_Init();
	// Бесконечный цикл задачи ReadData
	while (1)
	{
		//Здесь ожидание флага, чтобы запустить задачу ReadData
		flags = osEventFlagsWait(ReadDataEventHandle, FLAG_ReadData, osFlagsWaitAny, osWaitForever);
		// Новое значение счётчика времени
		TimeFromStart ++;

		for (int SensorNumber = 0; SensorNumber < SQ; SensorNumber++)
		{
			// Считывание с последовательной шины
			if (Sensor_array[SensorNumber].Active == 0) continue; // посылаем запрос только если датчик числится активным,
			MB_Master_Read(SensorNumber);
			// запись в очередь передачи данных в удалённый компьютер

			// запись в переменные экрана, если есть изменения
			// Temperature
			TempOld = Model::getCurrentVal_T(SensorNumber);
			TempNew = Sensor::GetData(TimeFromStart, SensorNumber, 2);
			if (TempOld != TempNew)
			{
				Model::setCurrentVal_T(SensorNumber, TempNew);
			}
			// Humidity
			HumOld = Model::getCurrentVal_H(SensorNumber);
			HumNew = Sensor::GetData(TimeFromStart, SensorNumber, 3);
			if (HumOld != HumNew)
			{
				Model::setCurrentVal_H(SensorNumber, HumNew);
			}
		// установка флага FLAG_DataAnalysis для запуска задачи DataAnalysis

		}
	}
}

// 3. The task DataAnalysis processing data from sensors
void DataFunc()
{
	osDelay(1000);
}

void InitData()
{

}
