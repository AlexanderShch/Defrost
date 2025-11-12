/*
 * Data.c
 *
 *  Created on: Jul 3, 2023
 *      Author: gdr
 */
#include <Data.hpp>
#include "main.h"
#include <string.h>


#include <gui\model\model.hpp>

// ReadDataEventHandle was defined in main.c
extern osEventFlagsId_t ReadDataEventHandle;
extern osEventFlagsId_t Start_TX_EventHandle;
extern SENSOR_typedef_t Sensor_array[SQ];
extern osThreadId_t TouchGFX_Task;
extern osMessageQueueId_t Data_QueueHandle;

uint32_t flags;				// flags for waiting event
int8_t SensorNumber;
uint16_t CirStop = 0b0001111000000000;	// стоповое слово, из которого будет выполняться перенос "бегущей единицы"
uint8_t CirNum= 4;						// счётчик паузы для цикла "бегущей единицы"
uint8_t ShiftCounter = DataRead_ShiftCounter;		// Счётчик 1 сек периодов между считываниями данных

/* Регистр аппаратного управления устройствами загружается в модуль ввода-вывода,
 * который содержит реле, переключающие устройства дефростера.
 * В регистр аппаратного управления загружаются флаги устройств из регистра состояния устройств.
 * Всего есть два регистра состояния устройств, флагами одного из которых управляет алгоритм,
 * а флагами другого - оператор (ручное управление).
 * Регистр состояни volatile - может изменяться другими потоками программы
*/
uint16_t RelayRegister = 0;				// Объявление регистра аппаратного управления устройствами

MB_Error_t result;

// definition of static variable. Member function definitions belong in the scope where the class is defined.
// current number of measure
unsigned int TimeFromStart = 0;
unsigned int Sensor::Time[TQ][SQ] = {{0}};	// number of time quantum measuring
int Sensor::T[TQ][SQ] = {{0}};		// temperature
int Sensor::H[TQ][SQ] = {{0}};		// humidity

typedef struct __attribute__((packed))   // object data for Server type
{
    uint8_t DataType;			// Байт типа передаваемых данных (0x00 для телеметрии)
    uint16_t Time;				// Количество секунд с момента включения
    uint8_t SensorQuantity;		// Количество сенсоров
    uint8_t SensorType[SQ];		// Тип сенсора
    uint8_t Active[SQ];			// Активность сенсора
    uint16_t T[SQ];				// Значение 1 сенсора (температура)
    uint16_t H[SQ];				// Значение 2 сенсора (влажность)
    uint16_t CRC_SUM;			// Контрольное значение
} MSGQUEUE_OBJ_t;
// !!! ВНИМАНИЕ! Если меняется структура, надо поменять и размер буфера MAX_MB_BUFSIZE в ModBus.cpp


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
	--ShiftCounter;
	if (ShiftCounter == 0) {
		ShiftCounter = DataRead_ShiftCounter;
		osEventFlagsSet(ReadDataEventHandle, FLAG_ReadData);
	};
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
	int T_CORR_Old, H_CORR_Old = 0, R_CORR_Old = 0;
	int T_CORR_New, H_CORR_New = 0, R_CORR_New = 0;
	MSGQUEUE_OBJ_t DataToServer;

	// ═══════════════════════════════════════════════════════════════════════════
	// КРИТИЧНО: Явно устанавливаем UART4 в режим приёма ПЕРЕД началом работы
	// Гарантируем, что DE pin в правильном состоянии для приёма команд от сервера
	// ═══════════════════════════════════════════════════════════════════════════
	HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_RESET);
	
	// Инициализация датчиков при запуске задачи
	MB_Master_Init();
	uint16_t *pDFR = (uint16_t*) &Model::DFR;	// указатель на регистр состояния устройств при авт.управлении DFR
	uint16_t *pDFR_manual = (uint16_t*) &Model::DFR_manual;	// указатель на регистр ручного управления устройствами DFR_manual
	uint16_t *pDFR_current = (uint16_t*) &Model::DFR_current;	// указатель на регистр текущего отображения устройств DFR_current
	uint16_t *pDFR_chng_flag = (uint16_t*) &Model::DFR_chng_flag;	// указатель на регистр DFR_chng_flag
	// Выведем на экран текущее состояние всех устройств в дефростере
	*pDFR = 0;
	*pDFR_manual = 0;
	*pDFR_current = 0;
	*pDFR_chng_flag = 0;
	// Бесконечный цикл задачи ReadData
	while (1)
	{
		//Здесь ожидание флага, чтобы запустить задачу ReadData
		flags = osEventFlagsWait(ReadDataEventHandle, FLAG_ReadData, osFlagsWaitAny, osWaitForever);
		// Новое значение счётчика времени
		TimeFromStart ++;

		/************************************************************
		 * Работа с регистрами управления устройствами
		 * RelayRegister записывается в регистр управления реле модуля ввода-вывода
		 * в цикле опроса датчиков
		 ************************************************************/
		if (Model::Flag_DFR_manual == 0) {
			// в режиме автоматического управления загрузим регистр управления устройствами
			RelayRegister = *pDFR;
		}else{
			// в ручном режиме грузим DFR_manual
			RelayRegister = *pDFR_manual;
		}
		// установим флаги изменения отображения на экране
		*pDFR_chng_flag = *pDFR ^ *pDFR_current;
		// загрузим новое значение в регистр текущего отображения устройств на экране
		*pDFR_current = *pDFR;

		/**************************************************************
		 * Цикл опроса датчиков
		 **************************************************************/
		for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
		{
			result = MB_ERROR_NO;
			// Считывание с последовательной шины:
			// посылаем запрос только если датчик числится активным!
			if (Sensor_array[SensorIndex].Active == 1)
			{
				// Продолжаем для активного датчика
				result = Sensor_Read(SensorIndex);

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
		}	// конец цикла опроса датчиков

		// формирование данных для сервера
		DataToServer = {};
		DataToServer.DataType = 0x00;	// Тип данных: 0x00 = телеметрия
		DataToServer.Time = TimeFromStart;
		DataToServer.SensorQuantity = SQ;
		for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
		{
			DataToServer.SensorType[SensorIndex] = Sensor_array[SensorIndex].TypeOfSensor;
			DataToServer.Active[SensorIndex] = Sensor_array[SensorIndex].Active;
			DataToServer.T[SensorIndex] = Sensor::GetData(TimeFromStart, SensorIndex, 2);
			DataToServer.H[SensorIndex] = Sensor::GetData(TimeFromStart, SensorIndex, 3);
		}
		// запись в очередь передачи данных в удалённый компьютер
		osMessageQueuePut(Data_QueueHandle, &DataToServer, 0U, 0U);

		// проверим не активные датчики на активность
		MB_Master_Init();

		// работа с корректировкой датчика
			if (Model::Flag_CORR_ready == 1) {
				T_CORR_Old = Model::T_CORR_sensor;
				H_CORR_Old = Model::H_CORR_sensor;
				R_CORR_Old = Model::R_CORR_sensor;
				// считаем параметры датчика
				result = Sensor_Read_CORR(Model::Index_CORR_sensor);
				// запишем корректировки в датчик
				if (Model::Flag_WR_to_sensor == 1)
				{
					result = Sensor_Write_CORR(Model::Index_CORR_sensor);
				}
				// обнулим корректировки в датчике типа 1
				if (Model::Flag_Alert == 1)
				{
					result = Sensor_CORR_Reset(Model::Index_CORR_sensor);
				}

				//  обновим значения датчиков на экране
				if (result == MB_ERROR_NO)
				{
					T_CORR_New = Model::T_CORR_sensor;
					H_CORR_New = Model::H_CORR_sensor;
					R_CORR_New = Model::R_CORR_sensor;
					if (T_CORR_Old != T_CORR_New) {
						Model::Flag_Corr_T_changed = 1;		// флаг для обновления данных на экране
					};
					if (H_CORR_Old != H_CORR_New) {
						Model::Flag_Corr_H_changed = 1;	// флаг для обновления данных на экране
					};
					if (R_CORR_Old != R_CORR_New) {
						Model::Flag_Corr_R_changed = 1;	// флаг для обновления данных на экране
					};
				}	// закончили считывать параметры с датчика
			}

			// установка флага FLAG_DataAnalysis для запуска задачи DataAnalysis

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

// ═══════════════════════════════════════════════════════════════════════════
// ХРАНЕНИЕ ПОСЛЕДНИХ ОТПРАВЛЕННЫХ ДАННЫХ ТЕЛЕМЕТРИИ
// ═══════════════════════════════════════════════════════════════════════════
static MSGQUEUE_OBJ_t LastSentTelemetry = {};  // Последние отправленные данные
static uint32_t TelemetryErrorCount = 0;       // Счётчик ошибок отправки телеметрии

/*
 * Функция: ResendLastTelemetry
 * Описание: Повторная отправка последних данных телеметрии
 * 
 * Используется когда сервер сообщает об ошибке в данных телеметрии (DATA_FALSE)
 * Отправляет последние сохранённые данные без изменений
 */
void ResendLastTelemetry(void)
{
	// Проверяем, есть ли сохранённые данные для повторной отправки
	if (LastSentTelemetry.DataType == 0x00)
	{
		// Отправляем сохранённые данные (CRC уже рассчитан)
		WriteToServer((uint8_t*)&LastSentTelemetry, (int) sizeof(LastSentTelemetry));
		
		// Инкрементируем счётчик ошибок
		TelemetryErrorCount++;
	}
}

// 4. Передача данных серверу работает в потоке TX_To_Server
void TX_ToServer()
{
	MSGQUEUE_OBJ_t Data_TX_Server;

	while (1)
	{
		Data_TX_Server = {};
		osMessageQueueGet(Data_QueueHandle, &Data_TX_Server, 0u, osWaitForever);
		// сделаем расчёт CRC для структуры Data_TX_Server,
		// но без учёта последних двух байт структуры, занятых CRC
		Data_TX_Server.CRC_SUM = MB_GetCRC((uint8_t*)&Data_TX_Server, sizeof(Data_TX_Server)-2);
		
		// ═══════════════════════════════════════════════════════════════════════════
		// СОХРАНЯЕМ данные телеметрии перед отправкой
		// Это позволит повторить отправку при ошибке (DATA_FALSE от сервера)
		// ═══════════════════════════════════════════════════════════════════════════
		memcpy(&LastSentTelemetry, &Data_TX_Server, sizeof(MSGQUEUE_OBJ_t));
		
		WriteToServer((uint8_t*)&Data_TX_Server, (int) sizeof(Data_TX_Server));
		if (result == MB_ERROR_NO)
		{
			// данные приняты - проверяем достоверность и сохраняем принятые данные в переменные
		}
	}
}


