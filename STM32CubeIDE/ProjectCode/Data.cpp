/*
 * Data.c
 *
 *  Создан: Jul 3, 2023
 *  Автор: gdr
 */
#include "Data.hpp"
#include "GateControl.hpp"
#include "DefrostControl.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>


#include <gui/model/Model.hpp>

// ReadDataEventHandle определён в main.c
extern osEventFlagsId_t ReadDataEventHandle;
extern osEventFlagsId_t Start_TX_EventHandle;
extern osSemaphoreId_t ServerResponseReceived_SemHandle;  // ответ на телеметрию (DATA_OK/DATA_FALSE)
extern osSemaphoreId_t SensorsReadDone_SemHandle;
extern SENSOR_typedef_t Sensor_array[SQ];
extern osThreadId_t TouchGFX_Task;


#define LOG_PACKET_SIZE  (2u + sizeof(ControlLogPayload_t) + 2u)

extern unsigned int TimeFromStart;  // определение ниже в файле

uint32_t flags;				// флаги для ожидания событий
int8_t SensorNumber;
uint16_t CirStop = 0b0001111000000000;	// стоповое слово, из которого будет выполняться перенос "бегущей единицы"
uint8_t CirNum= 4;						// счётчик паузы для цикла "бегущей единицы"

// Текущий интервал отправки телеметрии на сервер (сек).
// Почему: интервал меняется командой CFG_CMD_SET_INTERVAL и должен применяться без перезагрузки.
volatile uint16_t g_TelemetryIntervalSeconds = TELEMETRY_INTERVAL_DEFAULT_SEC;

void Telemetry_SetIntervalSeconds(uint16_t intervalSeconds)
{
	// Почему: защищаемся от нулевого интервала (зависание в постоянной отправке).
	if (intervalSeconds == 0)
	{
		intervalSeconds = TELEMETRY_INTERVAL_DEFAULT_SEC;
	}
	g_TelemetryIntervalSeconds = intervalSeconds;
}

// Подготовка данных текущей телеметрии
MSGQUEUE_OBJ_t Data_CurrentTelemetry(void)
{
	MSGQUEUE_OBJ_t DataToServer = {};
	DataToServer.Time = (uint16_t)TimeFromStart;
	DataToServer.SensorQuantity = SQ;
	for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
	{
		DataToServer.SensorType[SensorIndex] = Sensor_array[SensorIndex].TypeOfSensor;
		DataToServer.Active[SensorIndex] = Sensor_array[SensorIndex].Active;
		DataToServer.T[SensorIndex] = (int16_t)Sensor::GetData(TimeFromStart, SensorIndex, 2);
		DataToServer.H[SensorIndex] = (int16_t)Sensor::GetData(TimeFromStart, SensorIndex, 3);
	}
	return DataToServer;
}

/* Регистр аппаратного управления устройствами загружается в модуль ввода-вывода,
 * который содержит реле, переключающие устройства дефростера.
 * В регистр аппаратного управления загружаются флаги устройств из регистра состояния устройств.
 * Всего есть два регистра состояния устройств, флагами одного из которых управляет алгоритм,
 * а флагами другого - оператор (ручное управление).
 * Регистр состояни volatile - может изменяться другими потоками программы
*/
uint16_t RelayRegister = 0;				// Объявление регистра аппаратного управления устройствами

MB_Error_t result;

// Определение статических переменных.
// Текущее количество измерений (секунд от старта).
unsigned int TimeFromStart = 0;
unsigned int Sensor::Time[TQ][SQ] = {{0}};	// номер такта измерения
int Sensor::T[TQ][SQ] = {{0}};		// температура
int Sensor::T_Average[TQ][SQ] = {{0}};		// температура
int Sensor::H[TQ][SQ] = {{0}};		// влажность
// !!! ВНИМАНИЕ! Если меняется структура MSGQUEUE_OBJ_t, надо поменять и размер буфера MAX_MB_BUFSIZE в ModBus.cpp

/* Функция записывает int Val в массив данных, полученных с датчиков.
 * Параметр Param определяет, какой величиной массива является int Val.
 * Параметры функции:
 * 	TimeFromStart - кол-во тиков с момента запуска программы,
 * 	SensNum - number of interesting sensor,
 * 	Param - место int Val в массиве данных: 1 for time, 2 for temperature, 3 for humidity, 4 for average T
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
	case 4:
		T_Average[i][SensNum] = Val;
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
	case 2:
		return T[i][SensNum];
	case 3:
		return H[i][SensNum];
	case 4:
		return T_Average[i][SensNum];
	default:
		return 0;
	}
}

void Sensor::SetAverageTemperature(unsigned int TimeFromStart, unsigned char SensNum, int Temp, unsigned int windowSeconds)
{
	int TempAverage = 0;
	if (windowSeconds == 0u)
	{
		TempAverage = Temp;
	}

	long sum = 0;
	unsigned int count = 0;
	const unsigned int now = TimeFromStart;

	for (unsigned int idx = 0u; idx < TQ; ++idx)
	{
		const unsigned int t = Time[idx][SensNum];
		if (t == 0u)
		{
			continue;
		}
		const unsigned int dt = now - t;
		if (dt < windowSeconds)
		{
			sum += T[idx][SensNum];
			++count;
		}
	}

	if (count != 0u)
	{
		TempAverage = (int)(sum / (long)count);
	}
	PutData(TimeFromStart, SensNum, 4, TempAverage);
}

int ClatchSensorTemperature (unsigned int TimeFromStart, unsigned char SensNum, int Temp) {
	const int maxDeltaDeci = 11;
	int TempClatched = 0;
	int TempOld = 0;

	if (TimeFromStart == 0u)
	{
		TempClatched = Temp;
	}
	else
	{
		TempOld = Sensor::GetData(TimeFromStart-1, SensNum, 4); // предыдущее усредненное значение температуры
		int delta = Temp - TempOld;
		if (delta > maxDeltaDeci)
		{
			delta = maxDeltaDeci;
		}
		else if (delta < -maxDeltaDeci)
		{
			delta = -maxDeltaDeci;
		}
		TempClatched = TempOld + delta;
	}

	return TempClatched;
}

// 1. Operating system timer 1 sec will start this function
void DataTimerFunc()
{
	// Датчики должны считываться строго 1 раз в секунду.
	osEventFlagsSet(ReadDataEventHandle, FLAG_ReadData);
	// моргнём светодиодом
	HAL_GPIO_TogglePin(GPIOG, LD4_Pin);
	// Нельзя блокироваться внутри callback таймера RTOS: это добавляет джиттер и может задерживать другие таймеры.
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
	int TempClatched = 0;
	int T_CORR_Old, H_CORR_Old = 0, R_CORR_Old = 0;
	int T_CORR_New, H_CORR_New = 0, R_CORR_New = 0;

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
		// Выбираем активный регистр управления в зависимости от режима.
		const uint16_t activeRegister = (Model::Flag_DFR_manual == 0) ? *pDFR : *pDFR_manual;

		// В модуль ввода-вывода всегда отправляем активный регистр.
		RelayRegister = activeRegister;

		// Для визуализации всегда обновляем "текущее отображаемое состояние" активным регистром.
		// Это нужно, чтобы в ручном режиме в визуализации показывались вентиляторы и тэны.
		const uint16_t prevDisplayedRegister = *pDFR_current;
		*pDFR_chng_flag = activeRegister ^ prevDisplayedRegister;
		*pDFR_current = activeRegister;

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

				// Temperature: фильтруем по скорости изменения и усредняем по времени
				TempClatched = ClatchSensorTemperature(TimeFromStart, SensorIndex, Sensor::GetData(TimeFromStart, SensorIndex, 2));
				Sensor::SetAverageTemperature(TimeFromStart, SensorIndex, TempClatched, 10u);	// расчёт и запись в массив усредненное значение температуры за последние 10 секунд

				// запись в переменные экрана, если есть изменения
				TempOld = Model::getCurrentVal_T(SensorIndex);	// текущее значение температуры на экране
				TempNew = Sensor::GetData(TimeFromStart, SensorIndex, 2);	// новое значение (необработанное) температуры с датчика
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

		// Телеметрия и лог отправляются только по команде SEND_STATE от сервера (см. CommandReceiver REQ_CMD_SEND_STATE).
		/* отпускаем семафор SensorsReadDone для запуска задачи DataAnalysis
		 * osSemaphoreRelease — увеличивает счётчик семафора на 1 (отдаёт один «токен»).
		 * Это не «взвод», а именно «отпускание» семафора.
		 * Семафор «взводится» (становится доступным для ожидающих) именно вызовом Release, а не Acquire.
		 */
		if (SensorsReadDone_SemHandle != NULL) {
		    osSemaphoreRelease(SensorsReadDone_SemHandle);
		}

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

	}	// конец рабочего цикла
}

void InitData()
{

}

// ═══════════════════════════════════════════════════════════════════════════
// ХРАНЕНИЕ ПОСЛЕДНЕГО ОТПРАВЛЕННОГО ПАКЕТА ТЕЛЕМЕТРИИ (для повтора при DATA_FALSE)
// Формат пакета: [Type][Code][Status][DataLen][Data...][CRC16]
// ═══════════════════════════════════════════════════════════════════════════
#define TELEMETRY_PACKET_MAX  (4u + 113u + 2u)
static uint8_t s_lastSentTelemetryPacket[TELEMETRY_PACKET_MAX];
static uint16_t s_lastSentTelemetryPacketLen = 0u;

void Data_SaveLastSentTelemetryPacket(const uint8_t* packet, uint16_t length)
{
	if (packet != NULL && length > 0u && length <= TELEMETRY_PACKET_MAX)
	{
		memcpy(s_lastSentTelemetryPacket, packet, length);
		s_lastSentTelemetryPacketLen = length;
	}
}

/*
 * Функция: ResendLastTelemetry
 * Описание: Повторная отправка сохранённого байтового пакета телеметрии.
 * Добавляет в начало маркер [AA 55] и передаёт в WriteToServer().
 */
void ResendLastTelemetry(void)
{
	if (s_lastSentTelemetryPacketLen == 0u)
	{
		return;
	}
	static uint8_t buf[2u + TELEMETRY_PACKET_MAX];

	buf[0] = SYNC_START_1;
	buf[1] = SYNC_START_2;
	memcpy(&buf[2], s_lastSentTelemetryPacket, s_lastSentTelemetryPacketLen);

	WriteToServer(buf, (int)(2u + s_lastSentTelemetryPacketLen));
}

// Единая очередь отправки на сервер (телеметрия, лог, ответы на команды)
void ServerTx_EnqueueHighPriority(const uint8_t* data, uint16_t length)
{
	if (data == NULL || length > (SERVER_TX_ITEM_SIZE - 2u))
	{
		return;
	}
	ServerTxItem_t item = {};
	item.PacketLength = (uint8_t)length;
	memcpy(item.data, data, length);

	osMessageQueuePut(Data_QueueHandle, &item, 0U, 0U);
}

// Ждём элемент из очереди Data_QueueHandle (таймаут 50 мс, чтобы не нагружать CPU)
// принимаем элемент из очереди, добавляем CRC и отправляем на сервер через WriteToServerWithSync
void TX_ToServer()
{
	ServerTxItem_t item = {};

	while (1)
	{
		item = {};
		osStatus_t st = osMessageQueueGet(Data_QueueHandle, &item, 0u, 50u);
		if (st != osOK)
		{
			continue;
		}

		uint16_t len = (uint16_t)item.PacketLength;
		if (len == 0)
		{
			continue;
		}

		WriteToServerWithSync(item.data, (int)len);

	}
}


