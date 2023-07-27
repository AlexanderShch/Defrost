/*
 * Data.c
 *
 *  Created on: Jul 3, 2023
 *      Author: gdr
 */
#include <Data.hpp>
#include "main.h"
#include "cmsis_os.h"

#include <gui\model\model.hpp>

// ReadDataEventHandle was defined in main.c
extern osEventFlagsId_t ReadDataEventHandle;

uint32_t CurrentTime = 0;	// current number of measure
uint32_t flags;				// flags for waiting event
int8_t SensorNumber;
static float T=0, H=0;

extern "C"
{
	void DataTimerFunc_C()	// start from osTimer 1 sec
	{
		DataTimerFunc();
	}
	void ReadDataFunc_C()	// start ReadData after timer
	{
		ReadDataFunc();
	}
	void DataFunc_C()		// start DataAnalysis
	{
		DataFunc();
	}
	void InitDataVariables_C()
	{
		InitData();
	}
}

class Sensor
{
public:
	Sensor(){};										// declare default constructor
	Sensor(uint32_t Time, float T, float H){};		// declare constructor
	static void PutData(uint32_t TimeFromStart, int8_t SensNum, int8_t Param, float Val);
	static float GetData(uint32_t TimeFromStart, int8_t SensNum, int8_t Param);
protected:
	static uint32_t Time[TQ][SQ];	// number of time quantum measuring
	static float T[TQ][SQ];			// temperature
	static float H[TQ][SQ];			// humidity
};


// definition of static variable. Member function definitions belong in the scope where the class is defined.
uint32_t Sensor::Time[TQ][SQ] = {{0}};	// number of time quantum measuring
float Sensor::T[TQ][SQ] = {{0}};		// temperature
float Sensor::H[TQ][SQ] = {{0}};		// humidity


// Return integer value from measure array
// TimeFromStart - value of measure counter give correct data only for last TQ measures
// SensNum - number of interesting sensor
// Param - 1 for time, 2 for temperature, 3 for humidity
// Val - value of data
void Sensor::PutData(uint32_t TimeFromStart, int8_t SensNum, int8_t Param, float Val) {
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
//	 Param - 1 for time, 2 for temperature, 3 for humidity
float Sensor::GetData(uint32_t TimeFromStart, int8_t SensNum, int8_t Param) {
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
	// Здесь тест на изменение температуры
	// Температура изменилась для всех датчиков
	T += 0.01;

	HAL_GPIO_TogglePin(GPIOG, LD4_Pin);
	// Здесь установка флага события для запуска задачи по считыванию данных
	osEventFlagsSet(ReadDataEventHandle, FLAG_ReadData);
	osDelay(100);
	HAL_GPIO_TogglePin(GPIOG, LD4_Pin);

}

/* 2. The task ReadData reading data from sensors
* 	0 - defroster left
* 	1 - defroster right
* 	2 - defroster center
*	3 - fish left
*	4 - fish right
*/
void ReadDataFunc() {
	//Здесь ожидание флага, чтобы запустить задачу ReadData
	flags = osEventFlagsWait(ReadDataEventHandle, FLAG_ReadData, osFlagsWaitAny, osWaitForever);
	// Новое значение счётчика времени
	CurrentTime ++;

	//Здесь код для считывания данных с датчиков:
	for (int SensorNumber = 0; SensorNumber < SQ; ++SensorNumber) {
		float Temp = T+SensorNumber;
		// Считывание с последовательной шины
		H = 50.78;
		// запись в массив данных
		Sensor::PutData(CurrentTime, SensorNumber, 1, CurrentTime);
		Sensor::PutData(CurrentTime, SensorNumber, 2, Temp);
		Sensor::PutData(CurrentTime, SensorNumber, 3, H);
		// запись в очередь передачи данных в удалённый компьютер

		// запись в переменные экрана, если есть изменения
		Model::setCurrentVal(SensorNumber, Temp);
	}
	// установка флага FLAG_DataAnalysis для запуска задачи DataAnalysis
}

// 3. The task DataAnalysis processing data from sensors
void DataFunc()
{
	osDelay(1000);
}

void InitData()
{

}
