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
MB_Error_t result;

// definition of static variable. Member function definitions belong in the scope where the class is defined.
// current number of measure
unsigned int TimeFromStart = 0;
unsigned int Sensor::Time[TQ][SQ] = {{0}};	// number of time quantum measuring
int Sensor::T[TQ][SQ] = {{0}};		// temperature
int Sensor::H[TQ][SQ] = {{0}};		// humidity
int Sensor::Read_Data_1 = 0;		// первый считанный из датчика параметр, обычно это Н
int Sensor::Read_Data_2 = 0;		// второй считанный из датчика параметр, обычно это Т


/* Функция записывает int Val в массив данных, полученных с датчиков.
 * Параметр Param определяет, какой величиной массива является int Val.
 * Параметры функции:
 * 	TimeFromStart - кол-во тиков с момента запуска программы,
 * 	SensNum - number of interesting sensor,
 * 	Param - место int Val в массиве данных: 1 for time, 2 for temperature, 3 for humidity
 * 	Val - value of data
 */
void Sensor::PutData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param, int Val) {
	// Преобразуем TimeFromStart в номер тика внутри буфера из последних TQ измерений
	uint32_t i = TimeFromStart % TQ;

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

/*	 Функция возвращает integer value из массива данных, полученных с датчиков
*	 Параметры функции:
*	 	TimeFromStart - кол-во тиков с момента запуска программы, но будет получена величина только из буфера последних измерений размером TQ
*	 	SensNum - number of interesting sensor
*	 	Param - 0 for active, 1 for time, 2 for temperature, 3 for humidity
*/
int Sensor::GetData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param) {
	// Преобразуем TimeFromStart в номер тика внутри буфера из последних TQ измерений
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
	int T_CORR_Old, HR_CORR_Old = 0;
	int T_CORR_New, HR_CORR_New = 0;

	// Инициализация датчиков при запуске задачи
	MB_Master_Init();
	// Бесконечный цикл задачи ReadData
	while (1)
	{
		//Здесь ожидание флага, чтобы запустить задачу ReadData
		flags = osEventFlagsWait(ReadDataEventHandle, FLAG_ReadData, osFlagsWaitAny, osWaitForever);
		// Новое значение счётчика времени
		TimeFromStart ++;

		for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
		{
			result = MB_ERROR_NO;
			// Считывание с последовательной шины:
			// посылаем запрос только если датчик числится активным!
			if (Sensor_array[SensorIndex].Active == 1)
			{
				// Продолжаем для активного датчика
				result = Sensor_Read(SensorIndex);
				// запись в очередь передачи данных в удалённый компьютер

				// запись в переменные экрана, если есть изменения
				// Temperature
				TempOld = Model::getCurrentVal_T(SensorIndex);
				TempNew = Sensor::GetData(TimeFromStart, SensorIndex, 2);
				if (TempOld != TempNew)
				{
					Model::setCurrentVal_T(SensorIndex, TempNew);
				}
				// Humidity
				HumOld = Model::getCurrentVal_H(SensorIndex);
				HumNew = Sensor::GetData(TimeFromStart, SensorIndex, 3);
				if (HumOld != HumNew)
				{
					Model::setCurrentVal_H(SensorIndex, HumNew);
				}
			}
		// работа с корректировкой датчика
			if (Model::Flag_CORR_ready == 1) {
				T_CORR_Old = Model::T_CORR_sensor;
				HR_CORR_Old = Model::HR_CORR_sensor;
				// считаем параметры датчика
				result = Sensor_Read_CORR_param(Model::Index_CORR_sensor);
				if (result == MB_ERROR_NO)
				{
					T_CORR_New = Model::T_CORR_sensor;
					HR_CORR_New = Model::HR_CORR_sensor;
					if (T_CORR_Old != T_CORR_New) {
						Model::Flag_Corr_T_changed = 1;		// флаг для обновления данных на экране
					};
					if (HR_CORR_Old != HR_CORR_New) {
						Model::Flag_Corr_HR_changed = 1;	// флаг для обновления данных на экране
					};
				}	// закончили считывать параметры с датчика
				// запишем корректировки в датчик
				if (Model::Flag_WR_to_sensor == 1)
				{
					Model::Flag_WR_to_sensor = 0;
					Model::Index_CORR_sensor;

				}
			}

			// установка флага FLAG_DataAnalysis для запуска задачи DataAnalysis

		}	// конец цикла опроса датчиков
	}	// конец рабочего цикла
}

// 3. The task DataAnalysis processing data from sensors
void DataFunc()
{
	osDelay(1000);
}

void InitData()
{

}
