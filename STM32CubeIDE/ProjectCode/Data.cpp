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
#include <string.h>


#include <gui/model/Model.hpp>

// ReadDataEventHandle определён в main.c
extern osEventFlagsId_t ReadDataEventHandle;
extern osEventFlagsId_t Start_TX_EventHandle;
extern osSemaphoreId_t ServerResponseReceived_SemHandle;  // ответ на телеметрию (DATA_OK/DATA_FALSE)
extern SENSOR_typedef_t Sensor_array[SQ];
extern osThreadId_t TouchGFX_Task;

typedef struct __attribute__((packed))   // формат данных для сервера
{
    uint8_t DataType;			// Байт типа передаваемых данных (0x00 для телеметрии)
    uint8_t Len;               // Длина полезной части после Len и до CRC (в байтах), включается в CRC
    uint16_t Time;				// Количество секунд с момента включения
    uint8_t SensorQuantity;		// Количество сенсоров
    uint8_t SensorType[SQ];		// Тип сенсора
    uint8_t Active[SQ];			// Активность сенсора
    int16_t T[SQ];				// Значение 1 сенсора (температура)
    int16_t H[SQ];				// Значение 2 сенсора (влажность)
    uint16_t CRC_SUM;			// Контрольное значение
} MSGQUEUE_OBJ_t;
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

/* По команде SEND_STATE от сервера: сформировать текущую телеметрию и поставить в очередь. */
void Data_EnqueueCurrentTelemetry(void)
{
	MSGQUEUE_OBJ_t DataToServer = {};
	DataToServer.DataType = 0x00;
	DataToServer.Len = 45;
	DataToServer.Time = (uint16_t)TimeFromStart;
	DataToServer.SensorQuantity = SQ;
	for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
	{
		DataToServer.SensorType[SensorIndex] = Sensor_array[SensorIndex].TypeOfSensor;
		DataToServer.Active[SensorIndex] = Sensor_array[SensorIndex].Active;
		DataToServer.T[SensorIndex] = (int16_t)Sensor::GetData(TimeFromStart, SensorIndex, 2);
		DataToServer.H[SensorIndex] = (int16_t)Sensor::GetData(TimeFromStart, SensorIndex, 3);
	}
	ServerTxItem_t item = {};
	item.type = (uint8_t)SERVER_TX_TYPE_TELEMETRY;
	item.length = (uint8_t)sizeof(MSGQUEUE_OBJ_t);
	memcpy(item.data, &DataToServer, sizeof(MSGQUEUE_OBJ_t));
	osMessageQueuePut(Data_QueueHandle, &item, 0U, 0U);
}

/* По команде SEND_STATE: если авторежим — сформировать лог и поставить в очередь. */
void Data_EnqueueCurrentLogIfAuto(void)
{
	if (DefrostControl_IsEnabled() == 0 || GateControl_GetManualMode() != 0 || Model::isDefrostManualModeEnabled())
	{
		return;
	}
	ControlLogPayload_t logPayload;
	DefrostControl_GetControlLogPayload(&logPayload, (uint16_t)TimeFromStart);
	uint8_t logPacket[LOG_PACKET_SIZE];
	logPacket[0] = 0x01u;
	logPacket[1] = (uint8_t)sizeof(ControlLogPayload_t);
	memcpy(logPacket + 2, &logPayload, sizeof(ControlLogPayload_t));
	uint16_t crc = MB_GetCRC(logPacket, 2u + (uint16_t)sizeof(ControlLogPayload_t));
	logPacket[2 + sizeof(ControlLogPayload_t)] = (uint8_t)(crc & 0xFFu);
	logPacket[2 + sizeof(ControlLogPayload_t) + 1] = (uint8_t)(crc >> 8);
	ServerTxItem_t item = {};
	item.type = (uint8_t)SERVER_TX_TYPE_LOG;
	item.length = (uint8_t)LOG_PACKET_SIZE;
	memcpy(item.data, logPacket, LOG_PACKET_SIZE);
	osMessageQueuePut(Data_QueueHandle, &item, 0U, 0U);
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
int Sensor::H[TQ][SQ] = {{0}};		// влажность
// !!! ВНИМАНИЕ! Если меняется структура MSGQUEUE_OBJ_t, надо поменять и размер буфера MAX_MB_BUFSIZE в ModBus.cpp

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
	case 2:
		return T[i][SensNum];
	case 3:
		return H[i][SensNum];
	default:
		return 0;
	}
}

int Sensor::GetAverageTemperature(unsigned int TimeFromStart, unsigned char SensNum, unsigned int windowSeconds)
{
	if (windowSeconds == 0u)
	{
		return GetData(TimeFromStart, SensNum, 2);
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

	if (count == 0u)
	{
		return GetData(TimeFromStart, SensNum, 2);
	}

	return (int)(sum / (long)count);
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
	int HumNew = 0;
	int TempAvg10 = 0;
	int TempFiltered = 0;
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

				// запись в переменные экрана, если есть изменения
				// Temperature: фильтруем по скорости изменения и усредняем по времени
				TempOld = Model::getCurrentVal_T(SensorIndex);
				TempAvg10 = Sensor::GetAverageTemperature(TimeFromStart, SensorIndex, 10u);

				// Ограничение скорости изменения: не более 1.1 °C/сек (11 десятых градуса)
				const int maxDeltaDeci = 11;
				if (TimeFromStart == 0u || TempOld == 0)
				{
					TempFiltered = TempAvg10;
				}
				else
				{
					int delta = TempAvg10 - TempOld;
					if (delta > maxDeltaDeci)
					{
						delta = maxDeltaDeci;
					}
					else if (delta < -maxDeltaDeci)
					{
						delta = -maxDeltaDeci;
					}
					TempFiltered = TempOld + delta;
				}

				if (TempOld != TempFiltered)
				{
					Model::setCurrentVal_T(SensorIndex, TempFiltered);
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

		// Сначала обновляем состояние ворот (концевики/тайм-ауты),
		// затем шаг автоматики, чтобы автоматика видела актуальное состояние ворот в этом же такте.
		GateControl_Update1s();
		DefrostControl_Update1s();

		// Телеметрия и лог отправляются только по команде SEND_STATE от сервера (см. CommandReceiver REQ_CMD_SEND_STATE).

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
static MSGQUEUE_OBJ_t LastSentTelemetry = {};  // Последние отправленные данные (для повтора при DATA_FALSE)

/*
 * Функция: ResendLastTelemetry
 * Описание: Повторная отправка последних данных телеметрии
 *
 * Используется когда сервер сообщает об ошибке в данных телеметрии (DATA_FALSE)
 */
void ResendLastTelemetry(void)
{
	if (LastSentTelemetry.DataType != 0x00)
	{
		return;
	}
	ServerTx_EnqueueHighPriority((uint8_t*)&LastSentTelemetry, (uint16_t)sizeof(LastSentTelemetry));
}

// Единая очередь отправки на сервер (телеметрия, лог, ответы на команды)
void ServerTx_EnqueueNormal(ServerTxType_t type, const uint8_t* data, uint16_t length)
{
	if (data == NULL || length > (SERVER_TX_ITEM_SIZE - 2u))
	{
		return;
	}
	ServerTxItem_t item = {};
	item.type = (uint8_t)type;
	item.length = (uint8_t)length;
	memcpy(item.data, data, length);
	osMessageQueuePut(Data_QueueHandle, &item, 0U, 0U);
}

void ServerTx_EnqueueHighPriority(const uint8_t* data, uint16_t length)
{
	if (data == NULL || length > (SERVER_TX_ITEM_SIZE - 2u))
	{
		return;
	}
	ServerTxItem_t item = {};
	item.type = (uint8_t)SERVER_TX_TYPE_HIGH;
	item.length = (uint8_t)length;
	memcpy(item.data, data, length);
	osMessageQueuePut(Data_QueueHandle, &item, 0U, 0U);
}

// Передача данных серверу: одна очередь, отправка сразу при возможности (когда нет приёма — WriteToServerWithSync ждёт)
void TX_ToServer()
{
	ServerTxItem_t item = {};

	while (1)
	{
		item = {};
		// Ждём элемент из единственной очереди (таймаут 50 мс, чтобы не нагружать CPU)
		osStatus_t st = osMessageQueueGet(Data_QueueHandle, &item, 0u, 50u);
		if (st != osOK)
		{
			continue;
		}

		uint16_t len = (uint16_t)item.length;
		if (len == 0)
		{
			continue;
		}

		if (item.type == SERVER_TX_TYPE_TELEMETRY)
		{
			// CRC и копия для повтора при DATA_FALSE
			MSGQUEUE_OBJ_t* p = (MSGQUEUE_OBJ_t*)item.data;
			p->CRC_SUM = MB_GetCRC((uint8_t*)p, sizeof(MSGQUEUE_OBJ_t) - 2u);
			memcpy(&LastSentTelemetry, p, sizeof(MSGQUEUE_OBJ_t));
		}

		// Отправка сразу при возможности:
		// - для SERVER_TX_TYPE_HIGH используем WriteToServerWithSyncHighPriority (не блокируемся на CommandReceiver_IsHandling);
		// - для телеметрии/лога используем обычный WriteToServerWithSync.
		if (item.type == SERVER_TX_TYPE_HIGH)
		{
			WriteToServerWithSyncHighPriority(item.data, (int)len);
		}
		else
		{
			WriteToServerWithSync(item.data, (int)len);
		}

		// После отправки телеметрии ждём до 100 мс ответ сервера (DATA_OK/DATA_FALSE), чтобы не начинать передачу лога до приёма ответа (RS-485 half-duplex).
		if (item.type == SERVER_TX_TYPE_TELEMETRY && ServerResponseReceived_SemHandle != NULL)
		{
			while (osSemaphoreAcquire(ServerResponseReceived_SemHandle, 0u) == osOK)
			{
				;
			}
			osSemaphoreAcquire(ServerResponseReceived_SemHandle, 100u);
		}
	}
}


