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
extern osSemaphoreId_t ServerResponseReceived_SemHandle;  // ответ на телеметрию (DATA_OK/DATA_FALSE)
extern osSemaphoreId_t SensorsReadDone_SemHandle;
extern SENSOR_typedef_t Sensor_array[SQ];
extern osThreadId_t TouchGFX_Task;


#define LOG_PACKET_SIZE  (2u + sizeof(ControlLogPayload_t) + 2u)

uint32_t flags;				// флаги для ожидания событий
int8_t SensorNumber;
uint16_t CirStop = 0b0001111000000000;	// стоповое слово, из которого будет выполняться перенос "бегущей единицы"
uint8_t CirNum= 4;						// счётчик паузы для цикла "бегущей единицы"

// Последний полностью записанный в кольцо такт (0 = ещё не было завершённого опроса).
volatile unsigned int g_CommittedTick = 0;

// Подготовка данных текущей телеметрии
MSGQUEUE_OBJ_t Data_CurrentTelemetry(void)
{
	MSGQUEUE_OBJ_t DataToServer = {};
	static_assert(sizeof(MSGQUEUE_OBJ_t) == 60u, "MSGQUEUE_OBJ_t size must stay in sync with ProjectServerW");
	// Только committed-тик: иначе GetData(TimeFromStart) может отдать слот N%TQ со значениями N-TQ.
	const unsigned int tick = g_CommittedTick;
	DataToServer.Time = (uint16_t)tick;
	DataToServer.SensorQuantity = SQ;
	for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
	{
		DataToServer.SensorType[SensorIndex] = Sensor_array[SensorIndex].TypeOfSensor;
		DataToServer.Active[SensorIndex] = Sensor_array[SensorIndex].Active;
		DataToServer.RxErrorCnt[SensorIndex] = Sensor_array[SensorIndex].RxErrorCnt;
		if (tick == 0u || Sensor_array[SensorIndex].Active == 0u)
		{
			// Нет завершённого опроса или датчик неактивен — маркер отсутствия данных.
			DataToServer.T[SensorIndex] = SENSOR_NO_DATA_MARKER;
			DataToServer.H[SensorIndex] = SENSOR_NO_DATA_MARKER;
		}
		else
		{
			DataToServer.T[SensorIndex] = (int16_t)Sensor::GetData(tick, SensorIndex, 2);
			DataToServer.H[SensorIndex] = (int16_t)Sensor::GetData(tick, SensorIndex, 3);
		}
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

static void DetectSensorCommunicationAlarmsAfterPoll(void)
{
	// Индекс 6 — модуль IO, не датчик: в старом режиме его аварию не регистрируем.
	if (DefrostControl_UsesNewWrkAlrAlgorithm() == 0u && SQ > 6)
	{
		s_sensorCommLostMask &= (uint16_t)~(1u << 6);
		Model::Sensor_AlarmFlags &= (uint16_t)~(1u << 6);
		s_rxErrorStreak[6] = 0u;
	}
	// После повторного опроса ранее потерянный датчик может снова стать активным.
	for (int sensorIndex = 0; sensorIndex < SQ; ++sensorIndex)
	{
		if (sensorIndex >= 16) continue;
		const uint16_t sensorBit = (uint16_t)(1u << sensorIndex);
		if ((s_sensorCommLostMask & sensorBit) != 0u && Sensor_array[sensorIndex].Active == 1u)
		{
			s_sensorCommLostMask &= (uint16_t)~sensorBit;
			Model::Sensor_AlarmFlags &= (uint16_t)~sensorBit;
			s_rxErrorStreak[sensorIndex] = 0u;
		}
	}
	if (s_sensorCommLostMask != 0u)
		Model::Device_AlarmFlags |= kDeviceAlarmSensorCommBit;
	else
		Model::Device_AlarmFlags &= (uint16_t)~kDeviceAlarmSensorCommBit;
}

static uint16_t HandleDeviceAlarmsForOutput(uint16_t commandRegister)
{
	// Только новый режим: отказавшие исполнительные устройства не включаем.
	// Алгоритм при этом не останавливается (аварийный стоп авто — только ворота).
	if (DefrostControl_UsesNewWrkAlrAlgorithm() == 0u || Model::Flag_DFR_manual != 0u)
		return commandRegister;

	const uint16_t alarmBits = (uint16_t)(Model::Device_AlarmFlags & 0x01FFu);
	uint16_t forceOffMask = alarmBits; // аварийный вентилятор/ТЭН/вытяжка — бит в DO гасим

	// Авария вентилятора стороны: гасим оба ТЭНа этой стороны сразу в команде DO
	// (иначе до EnforceHeaterInterlockByFans после чтения DI ТЭНы ещё ~1 с остаются включёнными).
	const uint16_t kLeftFans  = (uint16_t)((1u << 0) | (1u << 1));
	const uint16_t kRightFans = (uint16_t)((1u << 2) | (1u << 3));
	const uint16_t kLeftTens  = (uint16_t)((1u << 4) | (1u << 5));
	const uint16_t kRightTens = (uint16_t)((1u << 6) | (1u << 7));
	if ((alarmBits & kLeftFans) != 0u)
		forceOffMask |= kLeftTens;
	if ((alarmBits & kRightFans) != 0u)
		forceOffMask |= kRightTens;

	return (uint16_t)(commandRegister & (uint16_t)~forceOffMask);
}

// Определение статических переменных.
// Текущее количество измерений (секунд от старта).
unsigned int TimeFromStart = 0;
unsigned int Sensor::Time[TQ][SQ] = {{0}};	// номер такта измерения
int Sensor::T[TQ][SQ] = {{0}};		// сырая температура (Param 2)
int Sensor::T_Clamped[TQ][SQ] = {{0}};	// обрезанная по шагу ΔT (антиспайк)
int8_t Sensor::T_ClampHit[TQ][SQ] = {{0}}; // знак/факт обрезки на такте (продукт −1/0/+1, воздух 0/1)
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

static constexpr unsigned char kSensProductLeft = 3u;
static constexpr unsigned char kSensProductRight = 4u;
/** Для продукта: меньшее из nPos/nNeg не ниже этого — считаем двусторонним шумом, а не тепловой переходной. */
static constexpr unsigned kClampNoiseMinorityMin = 3u;

static bool IsProductTempSensor(unsigned char sensNum)
{
	return (sensNum == kSensProductLeft) || (sensNum == kSensProductRight);
}

/** T-переход продукта: устойчивый знак (сырая − фильтрованная), не по обрезкам антиспайка. */
static constexpr int kProductChaseEnterDeci = 15;   // вход: |сырая − фильтр| > 1.5 °C
static constexpr int kProductChaseExitDeci = 8;     // выход: |сырая − фильтр| < 0.8 °C
static constexpr uint8_t kProductChaseEnterSteps = 5u;
static constexpr uint8_t kProductChaseExitSteps = 8u;
static int8_t s_productChaseDir[SQ] = {0};
static uint8_t s_productConsecUp[SQ] = {0};
static uint8_t s_productConsecDown[SQ] = {0};
static uint8_t s_productConsecClear[SQ] = {0};

static void ResetProductChaseState(unsigned char SensNum)
{
	if (SensNum >= SQ)
		return;
	s_productChaseDir[SensNum] = 0;
	s_productConsecUp[SensNum] = 0;
	s_productConsecDown[SensNum] = 0;
	s_productConsecClear[SensNum] = 0;
}

static void UpdateProductChaseFromRawVsFilt(unsigned char SensNum, int rawDeci, int filtDeci)
{
	if (!IsProductTempSensor(SensNum))
		return;
	if (Sensor_array[SensNum].Active != 1u)
	{
		ResetProductChaseState(SensNum);
		return;
	}
	const int d = rawDeci - filtDeci;
	if (d > kProductChaseEnterDeci)
	{
		if (s_productConsecUp[SensNum] < 255u)
			++s_productConsecUp[SensNum];
		s_productConsecDown[SensNum] = 0;
		s_productConsecClear[SensNum] = 0;
		if (s_productConsecUp[SensNum] >= kProductChaseEnterSteps)
			s_productChaseDir[SensNum] = 1;
	}
	else if (d < -kProductChaseEnterDeci)
	{
		if (s_productConsecDown[SensNum] < 255u)
			++s_productConsecDown[SensNum];
		s_productConsecUp[SensNum] = 0;
		s_productConsecClear[SensNum] = 0;
		if (s_productConsecDown[SensNum] >= kProductChaseEnterSteps)
			s_productChaseDir[SensNum] = (int8_t)-1;
	}
	else if ((d > -kProductChaseExitDeci) && (d < kProductChaseExitDeci))
	{
		if (s_productConsecClear[SensNum] < 255u)
			++s_productConsecClear[SensNum];
		s_productConsecUp[SensNum] = 0;
		s_productConsecDown[SensNum] = 0;
		if (s_productConsecClear[SensNum] >= kProductChaseExitSteps)
			s_productChaseDir[SensNum] = 0;
	}
	else
	{
		s_productConsecUp[SensNum] = 0;
		s_productConsecDown[SensNum] = 0;
		s_productConsecClear[SensNum] = 0;
	}
}

/** Ограничение шага изменения сырой T относительно предыдущей обрезанной (подавление выбросов), единицы — десятые °C.
 *  outHitSign: +1 нагрев быстрее порога, −1 охлаждение, 0 без обрезки. */
static int ClampRawTemperatureVsPrevClamped(int rawDeci, int prevClampedDeci, int8_t* outHitSign)
{
	const int maxDeltaDeci = 11;
	int delta = rawDeci - prevClampedDeci;
	int8_t hitSign = 0;
	if (delta > maxDeltaDeci)
	{
		delta = maxDeltaDeci;
		hitSign = 1;
	}
	else if (delta < -maxDeltaDeci)
	{
		delta = -maxDeltaDeci;
		hitSign = -1;
	}
	if (outHitSign != NULL)
		*outHitSign = hitSign;
	return prevClampedDeci + delta;
}

static bool SensorIndexIsTempThForClampAlarm(unsigned char sensNum)
{
	if (sensNum >= SQ)
		return false;
	const uint8_t t = Sensor_array[sensNum].TypeOfSensor;
	return (t == 1u || t == 2u);
}

void Sensor::CountClampHitSigns(unsigned char SensNum, unsigned* nPos, unsigned* nNeg)
{
	unsigned pos = 0u;
	unsigned neg = 0u;
	for (unsigned int idx = 0u; idx < TQ; ++idx)
	{
		const int8_t s = T_ClampHit[idx][SensNum];
		if (s > 0)
			++pos;
		else if (s < 0)
			++neg;
	}
	if (nPos != NULL)
		*nPos = pos;
	if (nNeg != NULL)
		*nNeg = neg;
}

uint8_t Sensor::IsProductThermalTransient(unsigned char SensNum)
{
	return (GetProductThermalChaseDir(SensNum) != 0) ? 1u : 0u;
}

int8_t Sensor::GetProductThermalChaseDir(unsigned char SensNum)
{
	if (!IsProductTempSensor(SensNum))
		return 0;
	if (Sensor_array[SensNum].Active != 1u)
	{
		ResetProductChaseState(SensNum);
		return 0;
	}
	return s_productChaseDir[SensNum];
}

uint8_t Sensor::IsProductTransitionRateBelowNoise(unsigned char SensNum)
{
	if (!IsProductTempSensor(SensNum))
		return 0u;
	return (GetProductThermalChaseDir(SensNum) == 0) ? 1u : 0u;
}

uint8_t Sensor::IsProductFalloutPending(unsigned char SensNum)
{
	if (!IsProductTempSensor(SensNum))
		return 0u;
	if (Sensor_array[SensNum].Active != 1u)
		return 0u;
	const int rawDeci = GetData(TimeFromStart, SensNum, 2);
	const int filtDeci = GetData(TimeFromStart, SensNum, 4);
	return ((rawDeci - filtDeci) > kProductChaseEnterDeci) ? 1u : 0u;
}

/** Воздух: защёлка при sumHits > TQ/2. Продукт: авария только при двустороннем шуме; иначе бит снимается. */
void Sensor::EvaluateClampAlarmForSensor(unsigned char SensNum)
{
	if (!SensorIndexIsTempThForClampAlarm(SensNum))
		return;

	const uint16_t sensorBit = (uint16_t)(1u << SensNum);

	if (IsProductTempSensor(SensNum))
	{
		unsigned nPos = 0u;
		unsigned nNeg = 0u;
		CountClampHitSigns(SensNum, &nPos, &nNeg);
		const unsigned nHit = nPos + nNeg;
		const unsigned minority = (nPos < nNeg) ? nPos : nNeg;
		const uint8_t mixedNoise = ((TimeFromStart >= TQ) &&
			(nHit > (TQ / 2u)) &&
			(minority >= kClampNoiseMinorityMin)) ? 1u : 0u;
		if (mixedNoise != 0u)
			Model::Sensor_AlarmFlags |= sensorBit;
		else if (Sensor_array[SensNum].Active == 1)
			Model::Sensor_AlarmFlags &= (uint16_t)~sensorBit;
		return;
	}

	if (TimeFromStart < TQ)
		return;
	unsigned int sumHits = 0u;
	for (unsigned int idx = 0u; idx < TQ; ++idx)
	{
		if (T_ClampHit[idx][SensNum] != 0)
			++sumHits;
	}
	if (sumHits > (TQ / 2u))
		Model::Sensor_AlarmFlags |= sensorBit;
}

void Sensor::ApplyTemperatureClampedBufferAndAverage(unsigned char SensNum, unsigned int timeSec)
{
	const int rawDeci = GetData(timeSec, SensNum, 2);
	const uint32_t slot = timeSec % TQ;
	int prevClampedDeci = rawDeci;
	if (timeSec > 1u)
		prevClampedDeci = T_Clamped[(timeSec - 1u) % TQ][SensNum];
	int8_t hitSign = 0;
	const int clampedDeci = ClampRawTemperatureVsPrevClamped(rawDeci, prevClampedDeci, &hitSign);
	T_Clamped[slot][SensNum] = clampedDeci;
	if (SensorIndexIsTempThForClampAlarm(SensNum))
	{
		// Знак обрезки — только антиспайк и авария помехи. T-переход продукта — по сырой vs фильтр.
		T_ClampHit[slot][SensNum] = IsProductTempSensor(SensNum) ? hitSign : ((hitSign != 0) ? (int8_t)1 : (int8_t)0);
	}
	else
	{
		T_ClampHit[slot][SensNum] = 0;
	}
	SetAverageTemperature(SensNum);
	EvaluateClampAlarmForSensor(SensNum);
	if (IsProductTempSensor(SensNum))
		UpdateProductChaseFromRawVsFilt(SensNum, rawDeci, GetData(timeSec, SensNum, 4));
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
// Выполняет цикл шины: запись DO -> датчики Т/Н -> чтение DO -> пауза 5 мс -> чтение DI.
// Публикует завершённый такт для телеметрии и разрешает обработку данных алгоритмом.
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
	HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET);
	// Канал опроса датчиков (UART5) — abort/flush/семафоры до первого Sensor_Read в Init.
	MB_Master_RxChannelPrepare();

	// Первый и последующие опросы выполняются в общем цикле: DO -> Т/Н -> DO/DI.
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
		// Старый режим _Wrk/_Alr выдаёт ожидающий серверный импульс ровно в этот секундный снимок регистра.
		DefrostControl_PrepareWrkAlrOutputs1s();
		// Общий флаг аварии: авария ворот ИЛИ любые аварийные биты устройств/датчиков.
		const uint16_t sensorAlarmBits = (uint16_t)(kDeviceAlarmSensorCommBit |
			Model::kDeviceAlarmProductTUpLeft | Model::kDeviceAlarmProductTUpRight);
		Model::Device_Alarm = ((Model::Sensor_AlarmFlags != 0u) ||
			((Model::Device_AlarmFlags & sensorAlarmBits) != 0u) ||
			(DefrostControl_UsesNewWrkAlrAlgorithm() != 0u &&
				(Model::Gate_Alarm != 0u || (Model::Device_AlarmFlags & (uint16_t)~sensorAlarmBits) != 0u))) ? 1u : 0u;
		const uint16_t commandRegisterRaw = (Model::Flag_DFR_manual == 0) ? *pDFR : *pDFR_manual;
		uint16_t activeRegister = HandleDeviceAlarmsForOutput(commandRegisterRaw);

		// В модуль ввода-вывода всегда отправляем активный регистр.
		RelayRegister = activeRegister;	// эта переменная запишется в модуль ввода-вывода
		const uint16_t prevDisplayedRegister = *pDFR_current;	// получаем предыдущее значение регистра управления
		*pDFR_chng_flag = activeRegister ^ prevDisplayedRegister;	// вычисляем флаг изменений
		*pDFR_current = activeRegister;	// записываем активный регистр в регистр управления
		// Записываем выходы один раз до всех запросов Т/Н, даже если IO был неактивен.
		// При ошибке записи всё равно читаем фактические DO/DI в конце цикла.
		MB_IO_WriteOutputs(SQ - 1u, RelayRegister);

		/**************************************************************
		 * Цикл опроса датчиков
		 **************************************************************/
		for (int SensorIndex = 0; SensorIndex < SQ; SensorIndex++)
		{
			result = MB_ERROR_NO;
			// Все устройства, включая неактивные, опрашиваем один раз в этом же порядке.
			// Индексы 0..5 — Т/Н; последний индекс — чтение DO, пауза 5 мс, чтение DI.
			{
				result = Sensor_Read(SensorIndex);
				// Успешный ответ возвращает ранее отключённое устройство в активный опрос.
				if (result == MB_ERROR_NO)
					Sensor_array[SensorIndex].Active = 1u;
				// Для активных устройств накапливаем последовательные ошибки приёма.
				if (Sensor_array[SensorIndex].Active != 0u &&
					(result == MB_ERROR_UART_RECIEVE || result == MB_ERROR_DMA_RECIEVE))
				{
					if (s_rxErrorStreak[SensorIndex] < 255u)
					{
						s_rxErrorStreak[SensorIndex]++;
					}
					// Если ошибка приёма повторилась 3 раза подряд — отключаем датчик и фиксируем аварию.
					if (s_rxErrorStreak[SensorIndex] >= 3u)
					{
						Sensor_array[SensorIndex].Active = 0u;
						if (SensorIndex < 16 &&
							(SensorIndex != 6 || DefrostControl_UsesNewWrkAlrAlgorithm() != 0u))
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
				// Не обновляем обработанные значения и экран данными недоступного устройства.
				if (Sensor_array[SensorIndex].Active == 0u) continue;

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

		// Сводим потери и восстановления связи до публикации результатов цикла.
		DetectSensorCommunicationAlarmsAfterPoll();
		// Фиксируем такт только после всех PutData датчиков/ВВ и обновления доступности.
		g_CommittedTick = TimeFromStart;

		// Телеметрия и лог отправляются только по команде SEND_STATE от сервера (см. CommandReceiver REQ_CMD_SEND_STATE).
		/* отпускаем семафор SensorsReadDone для запуска задачи DataAnalysis
		 * osSemaphoreRelease — увеличивает счётчик семафора на 1 (отдаёт один «токен»).
		 * Это не «взвод», а именно «отпускание» семафора.
		 * Семафор «взводится» (становится доступным для ожидающих) именно вызовом Release, а не Acquire.
		 */
		if (SensorsReadDone_SemHandle != NULL) {
		    osSemaphoreRelease(SensorsReadDone_SemHandle);
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


