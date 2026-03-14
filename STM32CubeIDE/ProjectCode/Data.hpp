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

#define TQ 16				// размер буфера (кол-во тактов) для хранения измерений
#define SQ 7				// датчики TH дефростера (0-2) + датчики продукта (3, 4) + T корпуса (5) + MB_IO (6)
#define STQ 5				// количество типов датчиков и IO-модулей
#define FLAG_ReadData 1ul	// флаг события чтения данных 0x00000001ul
// Интервал отправки телеметрии и лога на сервер по умолчанию (сек). Лог передаётся только в автоматическом режиме.
#define TELEMETRY_INTERVAL_DEFAULT_SEC 10u

class Sensor
{
public:
	Sensor(){};										// конструктор по умолчанию
	Sensor(unsigned int Time, int T, int H){};		// конструктор
	static void PutData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param, int Val);
	static void SetAverageTemperature(unsigned int TimeFromStart, unsigned char SensNum, unsigned int windowSeconds);
	static int GetData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param);

protected:
	static unsigned int Time[TQ][SQ];	// номер такта измерения
	static int T[TQ][SQ];			// температура
	static int T_Avarage[TQ][SQ];			// температура отфильтрованная усреднённая
	static int H[TQ][SQ];			// влажность
};

// Функция повторной отправки последних данных телеметрии
void ResendLastTelemetry(void);

// Текущий интервал отправки телеметрии (сек). Изменяется командой CFG_CMD_SET_INTERVAL.
extern volatile uint16_t g_TelemetryIntervalSeconds;

// Установить новый интервал отправки телеметрии (сек).
// Почему: интервал меняется из обработчика команд и должен применяться сразу (сброс счётчика).
void Telemetry_SetIntervalSeconds(uint16_t intervalSeconds);

// ═══════════════════════════════════════════════════════════════════════════
// Единая очередь отправки на сервер (телеметрия, лог, ответы на команды).
// Один поток TX_ToServer забирает из очереди и вызывает WriteToServerWithSync при возможности (нет приёма).
// ═══════════════════════════════════════════════════════════════════════════
#define SERVER_TX_ITEM_SIZE  99u   /* размер элемента очереди (type + length + payload); лог: 2+93+2=97 байт (Type+Len+ControlLogPayload_t+CRC) */

typedef enum {
	SERVER_TX_TYPE_TELEMETRY = 0,
	SERVER_TX_TYPE_LOG       = 1,
	SERVER_TX_TYPE_HIGH      = 2   /* ответ на команду или повтор телеметрии */
} ServerTxType_t;

typedef struct __attribute__((packed)) {
	uint8_t type;
	uint8_t length;
	uint8_t data[SERVER_TX_ITEM_SIZE - 2u];
} ServerTxItem_t;

extern osMessageQueueId_t Data_QueueHandle;
extern osSemaphoreId_t DataAnalysisStart_SemHandle;

/* Поставить в очередь (телеметрия, лог). Вызывать из DataProcessing / таймера. */
void ServerTx_EnqueueNormal(ServerTxType_t type, const uint8_t* data, uint16_t length);

/* Поставить в очередь (ответ на команду, повтор телеметрии). Тот же буфер, тип SERVER_TX_TYPE_HIGH. */
void ServerTx_EnqueueHighPriority(const uint8_t* data, uint16_t length);

/* По команде SEND_STATE: сформировать текущую телеметрию и поставить в очередь. */
void Data_EnqueueCurrentTelemetry(void);

/* По команде SEND_STATE: если авторежим — сформировать лог и поставить в очередь. */
void Data_EnqueueCurrentLogIfAuto(void);

#endif /* DATA_HPP_ */



