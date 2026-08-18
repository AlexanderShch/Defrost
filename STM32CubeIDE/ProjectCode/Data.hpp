/*
 * Data.hpp
 *
 *  Создан: Jul 3, 2023
 *  Автор: gdr
 */

#ifndef DATA_HPP_
#define DATA_HPP_

#include <stdint.h>
#include "cmsis_os.h"
#include "ModBus.hpp"

#define TQ 16				// размер буфера (кол-во тактов) для хранения измерений при расчёте среднего значения температуры
#define SQ 7				// датчики TH дефростера (0-2) + датчики продукта (3, 4) + T корпуса (5) + MB_IO (6)
#define STQ 5				// количество типов датчиков и IO-модулей
#define FLAG_ReadData 1ul	// флаг события чтения данных 0x00000001ul

class Sensor
{
public:
	Sensor(){};										// конструктор по умолчанию
	Sensor(unsigned int Time, int T, int H){};		// конструктор
	static void PutData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param, int Val);
	static int GetData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param);
	/** Сырая T (Param 2) → обрезание попомехе → слот T_Clamped → среднее в Param 4. timeSec = TimeFromStart. */
	static void ApplyTemperatureClampedBufferAndAverage(unsigned char SensNum, unsigned int timeSec);
	/** Датчики продукта 3/4: однонаправленная тепловая догонка. */
	static uint8_t IsProductThermalTransient(unsigned char SensNum);
	/** Знак догонки продукта: +1 рост T (выпадение), −1 падение T (установка в продукт), 0 нет переходной. */
	static int8_t GetProductThermalChaseDir(unsigned char SensNum);
	/** Текущий шаг T продукта не обрезается (|dT| <= шумового порога). */
	static uint8_t IsProductTransitionRateBelowNoise(unsigned char SensNum);

protected:
	static void SetAverageTemperature(unsigned char SensNum);
	/** Подсчёт T_ClampHit по кольцу и защёлка/сброс Model::Sensor_AlarmFlags. */
	static void EvaluateClampAlarmForSensor(unsigned char SensNum);
	static void CountClampHitSigns(unsigned char SensNum, unsigned* nPos, unsigned* nNeg);
	static unsigned int Time[TQ][SQ];	// номер такта измерения
	static int T[TQ][SQ];			// сырая температура с датчика (Param 2): экран и телеметрия
	static int T_Clamped[TQ][SQ];		// после ограничения скорости изменения (антиспайк, обрезка по помехе); участвует в среднем
	static int8_t T_ClampHit[TQ][SQ];	// продукт: −1/0/+1 (знак обрезки); воздух: 0/1
	static int T_Average[TQ][SQ];		// отфильтрованная T (Param 4): алгоритм и лог
	static int H[TQ][SQ];			// влажность
};

// Функция повторной отправки последних данных телеметрии
void ResendLastTelemetry(void);

/* Сохранить готовый байтовый пакет телеметрии [Type][Code][Status][DataLen][Data...][CRC16] для повтора при TELEMETRY_DATA_FALSE. */
void Data_SaveLastSentTelemetryPacket(const uint8_t* packet, uint16_t length);

/* Последний такт, для которого данные в строку кольца T/H полностью записаны (после всех PutData опроса).
 * SEND_STATE / Data_CurrentTelemetry читают только этот тик — без гонки с незавершённым опросом. */
extern volatile unsigned int g_CommittedTick;

// ═══════════════════════════════════════════════════════════════════════════
// Единая очередь отправки на сервер (телеметрия, лог, ответы на команды).
// Один поток TX_ToServer забирает из очереди и вызывает WriteToServerWithSync при возможности (нет приёма).
// ═══════════════════════════════════════════════════════════════════════════
#define SERVER_TX_ITEM_SIZE  98u   /* размер элемента очереди (type + length + payload); регулярный лог только группа 3 ~68 байт */
// Маркер "нет данных" для телеметрии неактивного датчика.
#define SENSOR_NO_DATA_MARKER ((int16_t)-32768)

typedef struct __attribute__((packed))   // формат данных для сервера
{
    uint16_t Time;				// Количество секунд с момента включения
    uint8_t SensorQuantity;		// Количество сенсоров
    uint8_t SensorType[SQ];		// Тип сенсора
    uint8_t Active[SQ];			// Активность сенсора
    int16_t T[SQ];				// Значение 1 сенсора (температура / DI для IO)
    int16_t H[SQ];				// Значение 2 сенсора (влажность / DO для IO)
    uint8_t ShutdownActive;     // Флаг post-shutdown из алгоритма дефроста (0/1)
    uint16_t RxErrorCnt[SQ];	// Накопленные ошибки приёма ModBus (таймаут/CRC/FC) по каждому клиенту
} MSGQUEUE_OBJ_t;
/*
 * MSGQUEUE_OBJ_t packed: Time2 + SQ1 + Type SQ + Active SQ + T 2SQ + H 2SQ + Shd1 + RxErr 2SQ
 * = 3 + 6*SQ + 1 + 2*SQ = 4 + 8*SQ
 * SQ=7 → 4+56 = 60 байт.
 */
 
typedef enum {
	SERVER_TX_TYPE_TELEMETRY = 0,
	SERVER_TX_TYPE_LOG       = 1,
	SERVER_TX_TYPE_HIGH      = 2   /* ответ на команду или повтор телеметрии */
} ServerTxType_t;

// Структура пакета, отправляемого по шине на сервер
typedef struct __attribute__((packed)) {
	uint8_t PacketLength;
	uint8_t data[SERVER_TX_ITEM_SIZE - 2u];
} ServerTxItem_t;

extern osMessageQueueId_t Data_QueueHandle;

/* Поставить в очередь (ответ на команду, повтор телеметрии). Тот же буфер, тип SERVER_TX_TYPE_HIGH. */
void ServerTx_EnqueueHighPriority(const uint8_t* data, uint16_t length);

/* По команде SEND_STATE: сформировать текущую телеметрию и передать для постановки в очередь. */
MSGQUEUE_OBJ_t Data_CurrentTelemetry(void);

#endif /* DATA_HPP_ */



