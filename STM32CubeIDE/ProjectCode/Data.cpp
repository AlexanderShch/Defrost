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
    DataToServer.ShutdownActive = DefrostControl_IsShutdownActive();
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
static uint8_t s_rxErrorStreak[SQ] = {0};           // подряд идущие ошибки приёма по каждому датчику
static uint16_t s_sensorCommLostMask = 0u;          // датчики, отключённые из-за 3 подряд ошибок приёма
static const uint16_t kDeviceAlarmSensorCommBit = (uint16_t)(1u << 12); // агрегированный флаг "есть потерянный датчик"

// Определение статических переменных.
// Текущее количество измерений (секунд от старта).
unsigned int TimeFromStart = 0;
unsigned int Sensor::Time[TQ][SQ] = {{0}};	// номер такта измерения
int Sensor::T[TQ][SQ] = {{0}};		// сырая температура (Param 2)
int Sensor::T_Clamped[TQ][SQ] = {{0}};	// обрезанная по шагу ΔT (антиспайк)
uint8_t Sensor::T_ClampHit[TQ][SQ] = {{0}}; // факт обрезки на такте (для регистра аварии датчиков)
int Sensor::T_Average[TQ][SQ] = {{0}};	// усреднённая по буферу обрезанных (Param 4)
int Sensor::H[TQ][SQ] = {{0}};		// влажность
// !!! ВНИМАНИЕ! Если меняется структура MSGQUEUE_OBJ_t, надо поменять и размер буфера MAX_MB_BUFSIZE в ModBus.cpp

/* Функция записывает int Val в массив данных, полученных с датчиков.
 * Параметр Param определяет, какой величиной массива является int Val.
 * Параметры функции:
 * 	TimeFromStart - кол-во тиков с момента запуска программы,
 * 	SensNum - number of interesting sensor,
 * 	Param: 1 time, 2 сырая T (с шины), 3 H, 4 усреднённая по буферу обрезанных T (для алгоритма/лога)
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

void Sensor::SetAverageTemperature(unsigned char SensNum)
{
	long sum = 0;
	unsigned int count = 0;
	// Усреднение только по уже заполненным тактам буфера «обрезанных» T (секунды 1..TimeFromStart, пока кольцо не заполнилось).
	if (TimeFromStart <= TQ)
	{
		for (unsigned int sec = 1u; sec <= TimeFromStart; ++sec)
		{
			const uint32_t idx = sec % TQ;
			sum += (long)T_Clamped[idx][SensNum];
			++count;
		}
	}
	else
	{
		for (unsigned int idx = 0u; idx < TQ; ++idx)
		{
			sum += (long)T_Clamped[idx][SensNum];
			++count;
		}
	}
	const int TempAverage = (count > 0) ? (int)(sum / (long)count) : 0;
	PutData(TimeFromStart, SensNum, 4, TempAverage);
}

/** Ограничение шага изменения сырой T относительно предыдущей обрезанной (подавление выбросов), единицы — десятые °C.
 *  outHit != NULL: *outHit=1 если сырая T потребовала обрезки (|Δ| > max за шаг). */
static int ClampRawTemperatureVsPrevClamped(int rawDeci, int prevClampedDeci, uint8_t* outHit)
{
	const int maxDeltaDeci = 11;
	int delta = rawDeci - prevClampedDeci;
	const uint8_t hit = (delta > maxDeltaDeci || delta < -maxDeltaDeci) ? 1u : 0u;
	if (delta > maxDeltaDeci)
		delta = maxDeltaDeci;
	else if (delta < -maxDeltaDeci)
		delta = -maxDeltaDeci;
	if (outHit != NULL)
		*outHit = hit;
	return prevClampedDeci + delta;
}

static bool SensorIndexIsTempThForClampAlarm(unsigned char sensNum)
{
	if (sensNum >= SQ)
		return false;
	const uint8_t t = Sensor_array[sensNum].TypeOfSensor;
	return (t == 1u || t == 2u);
}

/** Если в полном кольце T_ClampHit больше TQ/2 единиц — защёлкнуть бит в Model::Sensor_AlarmFlags. */
void Sensor::EvaluateClampAlarmForSensor(unsigned char SensNum)
{
	if (!SensorIndexIsTempThForClampAlarm(SensNum))
		return;
	if (TimeFromStart < TQ)
		return;
	unsigned int sumHits = 0u;
	for (unsigned int idx = 0u; idx < TQ; ++idx)
		sumHits += (unsigned int)T_ClampHit[idx][SensNum];
	if (sumHits > (TQ / 2u))
		Model::Sensor_AlarmFlags |= (uint16_t)(1u << SensNum);
}

void Sensor::ApplyTemperatureClampedBufferAndAverage(unsigned char SensNum, unsigned int timeSec)
{
	const int rawDeci = GetData(timeSec, SensNum, 2);
	const uint32_t slot = timeSec % TQ;
	int prevClampedDeci = rawDeci;
	if (timeSec > 1u)
		prevClampedDeci = T_Clamped[(timeSec - 1u) % TQ][SensNum];
	uint8_t hit = 0u;
	const int clampedDeci = ClampRawTemperatureVsPrevClamped(rawDeci, prevClampedDeci, &hit);
	T_Clamped[slot][SensNum] = clampedDeci;
	if (SensorIndexIsTempThForClampAlarm(SensNum))
		T_ClampHit[slot][SensNum] = hit;
	else
		T_ClampHit[slot][SensNum] = 0u;
	SetAverageTemperature(SensNum);
	EvaluateClampAlarmForSensor(SensNum);
}

// 1. Operating system timer 1 sec will start this function
void DataTimerFunc()
{
	// Датчики должны считываться строго 1 раз в секунду.
	osEventFlagsSet(ReadDataEventHandle, FLAG_ReadData);
	// моргнём светодиодом (heartbeat работы контроллера)
	HAL_GPIO_TogglePin(GPIOG, LD3_Pin);
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
		// Общий флаг аварии: авария ворот ИЛИ любые аварийные биты устройств.
		Model::Device_Alarm = ((Model::Gate_Alarm != 0) || (Model::Device_AlarmFlags != 0) || (Model::Sensor_AlarmFlags != 0)) ? 1 : 0;
		const uint16_t commandRegisterRaw = (Model::Flag_DFR_manual == 0) ? *pDFR : *pDFR_manual;
		uint16_t activeRegister = commandRegisterRaw;
		// В автоматическом режиме блокируем включение аварийных устройств:
		// если по каналу зафиксирована авария подтверждения (Device_AlarmFlags), бит команды принудительно сбрасывается.
		if (Model::Flag_DFR_manual == 0)
		{
			const uint16_t deviceCheckMask = (uint16_t)((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
														(1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) |
														(1u << 8));
			const uint16_t blockedMask = (uint16_t)(Model::Device_AlarmFlags & deviceCheckMask);
			// аварийные устройства блокируются
			activeRegister &= (uint16_t)(~blockedMask);
		}

		// В модуль ввода-вывода всегда отправляем активный регистр.
		RelayRegister = activeRegister;	// эта переменная запишется в модуль ввода-вывода
		const uint16_t prevDisplayedRegister = *pDFR_current;	// получаем предыдущее значение регистра управления
		*pDFR_chng_flag = activeRegister ^ prevDisplayedRegister;	// вычисляем флаг изменений
		*pDFR_current = activeRegister;	// записываем активный регистр в регистр управления

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
				if (result == MB_ERROR_UART_RECIEVE || result == MB_ERROR_DMA_RECIEVE)
				{
					if (s_rxErrorStreak[SensorIndex] < 255u)
					{
						s_rxErrorStreak[SensorIndex]++;
					}
					// Если ошибка приёма повторилась 3 раза подряд — отключаем датчик и фиксируем аварию.
					if (s_rxErrorStreak[SensorIndex] >= 3u)
					{
						Sensor_array[SensorIndex].Active = 0u;
						if (SensorIndex < 16)
						{
							const uint16_t sensorBit = (uint16_t)(1u << SensorIndex);
							s_sensorCommLostMask |= sensorBit;
							Model::Sensor_AlarmFlags |= sensorBit;
						}
						Model::Device_AlarmFlags |= kDeviceAlarmSensorCommBit;
					}
				}
				else
				{
					s_rxErrorStreak[SensorIndex] = 0u;
				}

				// Температура: сырая T (Param 2) → антиспайк → буфер T_Clamped → среднее в Param 4 (алгоритм, лог).
				Sensor::ApplyTemperatureClampedBufferAndAverage((unsigned char)SensorIndex, TimeFromStart);

				// запись в переменные экрана, если есть изменения
				TempOld = Model::getCurrentVal_T(SensorIndex);	// текущее значение температуры на экране
				TempNew = Sensor::GetData(TimeFromStart, SensorIndex, 2);	// сырая T с датчика (телеметрия и UI)
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
		// Если ранее отключённый по связи датчик снова ответил и стал Active=1 — снимаем его аварийный признак.
		for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
		{
			if (SensorIndex < 16)
			{
				const uint16_t sensorBit = (uint16_t)(1u << SensorIndex);
				if (((s_sensorCommLostMask & sensorBit) != 0u) && (Sensor_array[SensorIndex].Active == 1u))	// если датчик снова активен, то сбрасываем аварийный бит в регистре Sensor_AlarmFlags
				{
					s_sensorCommLostMask &= (uint16_t)~sensorBit;		// сбрасываем аварийный бит в регистре Sensor_AlarmFlags
					Model::Sensor_AlarmFlags &= (uint16_t)~sensorBit;	// сбрасываем аварийный бит в регистре Sensor_AlarmFlags
					s_rxErrorStreak[SensorIndex] = 0u;				// сбрасываем счётчик ошибок приёма
				}
			}
		}
		if (s_sensorCommLostMask != 0u)		// если ещё остались отключённые датчики, то фиксируем аварийный бит в регистре Device_AlarmFlags
		{
			Model::Device_AlarmFlags |= kDeviceAlarmSensorCommBit;
		}
		else // если все датчики снова активны, то сбрасываем аварийный бит в регистре Device_AlarmFlags
		{
			Model::Device_AlarmFlags &= (uint16_t)~kDeviceAlarmSensorCommBit;
		}

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


