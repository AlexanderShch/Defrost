#include "main.h"
#include "cmsis_os.h"
#include "ModBus.hpp"
#include "Data.hpp"
#include "CommandReceiver.hpp"
#include "DefrostControl.h"
#include "stdio.h"
#include "task.h"
#include "string.h"
#include "queue.h"
#include <gui\model\model.hpp>

//#include "usb_host.h"

#define  MAX_MB_BUFSIZE 101						// максимальный размер пакета UART4 с SYNC: лог 0x01 только группа 3 (2+~68+2)+AA55; телеметрия и ответы меньше

extern "C" {
extern UART_HandleTypeDef huart4;				// UART4: программирование датчиков и связь с сервером
extern UART_HandleTypeDef huart5;				// UART5: опрос датчиков в рабочем режиме
extern osSemaphoreId_t TX_Compl_SemHandle;		// завершение передачи к датчикам
extern osSemaphoreId_t RX_Compl_SemHandle;		// завершение приёма от датчиков
extern osSemaphoreId_t PR_TX_Compl_SemHandle;	// завершение передачи UART4 (общий)
extern osSemaphoreId_t PR_RX_Compl_SemHandle;	// завершение приёма UART4 в режиме программирования
extern osSemaphoreId_t UART4_RX_Event_SemHandle; // событие UART4 RX-to-IDLE (для арбитража с сервером)
extern osMutexId_t UART4_MutexHandle;			// защита переходов владения UART4
}

// Арбитраж владения UART4 между сервером и программированием
typedef enum
{
	UART4_OWNER_SERVER = 0,
	UART4_OWNER_PROGRAMMING = 1
} UART4_Owner_t;

// Глобальная переменная владения UART4
static volatile UART4_Owner_t g_uart4Owner = UART4_OWNER_SERVER;

// Функция установки владения UART4 сервером
extern "C" void UART4_SetOwner_Server(void)
{
	g_uart4Owner = UART4_OWNER_SERVER;
}

// Функция установки владения UART4 программированием
extern "C" void UART4_SetOwner_Programming(void)
{
	g_uart4Owner = UART4_OWNER_PROGRAMMING;
}

// Функция проверки владения UART4 программированием
extern "C" uint8_t UART4_IsOwner_Programming(void)
{
	return (g_uart4Owner == UART4_OWNER_PROGRAMMING) ? 1 : 0;
}

// Объявления функций CommandReceiver для обработки данных от сервера
void CommandReceiver_OnDataReceived(uint16_t receivedSize);
void CommandReceiver_RestartReception(void);

extern unsigned int TimeFromStart;
extern uint16_t RelayRegister;	// временная переменная, заменяющая регистр аппаратного управления устройствами
												// volatile: может изменяться другими потоками

uint8_t MB_MasterTx_Buffer[MAX_MB_BUFSIZE] = {0};
uint8_t MB_MasterRx_Buffer[MAX_MB_BUFSIZE] = {0};
using MultWR_t = int8_t[8];						// Тип данных - буфер для данных множественных команд записи в устройство ModBus
MultWR_t WR_Buffer = {0};						// Объявление буфера для множественных команд

uint16_t master_rec_byte_count = 0;
uint8_t FrameDelay1 = 30;						// Задержка между фреймами рабочая
//uint8_t FrameDelay2 = 0;						// Задержка между фреймами дополнительная при смене типа датчика
/*
 * Задержка между фреймами подобрана опытным путём. По стандарту ModBus RTU "Перед началом передачи очередного фрейма, необходима выдержка времени,
 * соответствующая 3,5 временам передачи одного байта данных (t3,5) после завершения передачи предыдущего фрейма (или “ложной” передачи данных)."
 * Однако на практике датчики начали отвечать без сбоев только при удержании шины в свободном состоянии в течение 30 мс - это примерно 6 фреймов
 * (не байт, а фреймов) на скорости 19200.
*/

uint16_t MB_TransactionHandler();
osStatus_t resultSem;			/* статус семафора:  	osOK: токен получен, и количество токенов уменьшено.
														osErrorTimeout: не удалось получить токен в заданное время.
														osErrorResource: не удалось получить токен, если не был указан тайм-аут.
														osErrorParameter: параметр semaphore_id имеет значение NULL или недопустим */

/*Таблицы CRC16-CITT*/
const uint16_t crc16_table[] =
  {
    0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
    0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
    0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
    0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
    0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
    0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
    0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
    0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
    0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
    0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
    0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
    0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
    0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
    0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
    0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
    0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040,
  };

/*
 * Типы датчиков:
 */
SENSOR_Type_t Sensor_type[STQ] =
{
		{0, "Null"},			// 0 - нет датчика
		{1, "Double T&H"},		// 1 - совмещенный температура и влажность GL-TH04-MT
		{2, "Single T"},		// 2 - датчик температуры РТ100 с RS485
		{3, "BT T"},			// 3 - датчик температуры Bluetooth
		{4, "MB IO"}			// 4 - модуль ввода-вывода MB 16DI-16RO
};

SENSOR_typedef_t Sensor_array[SQ] =
{
		{101,3,0,1,"Left def", 0,0,0,0,1},		// 0 - дефростер левый, 	GL-TH04-MT
		{102,3,0,1,"Right def",0,0,0,0,1},		// 1 - дефростер правый,	GL-TH04-MT
		{103,3,0,1,"Center def",0,0,0,0,1},	// 2 - дефростер центральный, GL-TH04-MT
		{104,3,0,2,"Left prod",0,0,0,0,1},		// 3 - продукт левый, 		РТ100 с RS485
		{105,3,0,2,"Right prod",0,0,0,0,1},	// 4 - продукт правый,		РТ100 с RS485
		{106,3,0,2,"Body def",0,0,0,0,1},		// 5 - Т корпуса дефростера,	РТ100 с RS485
		{002,3,0,4,"MB 16IO",0,0,0,0,1}		// 6 - модуль ввода-вывода с RS485, диапазон адресов: 2 и 3
};

uint8_t SensNullValue = 255;
uint8_t SensPortNumber;					// физический адрес устройства на шине - получает значение из устройства при сканировании шины
uint8_t SensAddress;					// физический адрес устройства на шине - получает значение из массива Sensor_array по индексу
uint8_t SensBaudRateIndex;				// индекс в массиве скорости шины - получает значение из устройства при сканировании шины
int Sens_WR_value;						// переменная для чтения записанного в датчик значения, применяется при контроля после записи в датчик
// массив скорости шины для датчика типа 1 GL-TH04-MT
int BaudRate_Type1[BAUD_RATE_NUMBER] = {2400, 4800, 9600, 19200, 38400, 57600, 115200, 1200};
// массив скорости шины для датчика типа 2 PT100 PT21A01
int BaudRate_Type2[BAUD_RATE_NUMBER] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
// массив скорости шины для датчика типа 4 MB 16DI-16RO
int BaudRate_Type4[BAUD_RATE_NUMBER] = {2400, 4800, 9600, 19200, 38400, 57600, 115200, 1200};

typedef struct
{
	uint8_t Tx_Buffer[MAX_MB_BUFSIZE] = {0};		// буфер передачи
	uint8_t Rx_Buffer[MAX_MB_BUFSIZE] = {0};		// буфер приёма
	UART_HandleTypeDef *UART;
	GPIO_TypeDef *PORT;
	uint16_t PORT_PIN;
	osSemaphoreId_t *Sem_Tx;
	osSemaphoreId_t *Sem_Rx;
	uint16_t Read_Data_1;							// данные №1, считанные с шины
	uint16_t Read_Data_2;							// данные №2, считанные с шины
	uint8_t PreviosTypeOfSensor;					// тип предыдущего обработанного датчика

} MB_Active_t;										// среда работы датчика

MB_Error_t Master_Request(MB_Active_t *MB, int N_Bytes);
MB_Error_t Master_SendTelemetry(MB_Active_t *MB, int N_Bytes);
MB_Error_t Master_RW(MB_Active_t *MB, int Address, MB_Command_t CMD, MB_Reg_t START_REG, uint16_t DATA, MultWR_t WR_Buf);
//MB_Error_t Master_Read(MB_Active_t *MB, uint8_t SensIndex, MB_Command_t CMD, MB_Reg_t START_REG, uint8_t DATA);
MB_Error_t ScanSensor(MB_Active_t *MB);
MB_Error_t WriteToSensor(MB_Active_t *PR);
MB_Error_t CheckAndWaitForActiveReception(UART_HandleTypeDef *uart, osSemaphoreId_t *sem_rx);
// при чтении из датчика значение кол-ва переданных байт данных в Rx_Buffer[2] + всегда передаётся 5 байт
#define CheckAnswerCRC (MB->Rx_Buffer[1] == CMD && MB_GetCRC(MB->Rx_Buffer, MB->Rx_Buffer[2] + 5) == 0)
// при записи в датчик всегда передаётся 8 байт
#define PR_CheckAnswerCRC (MB->Rx_Buffer[1] == CMD && MB_GetCRC(MB->Rx_Buffer, 8) == 0)
int Parametr_CORR;

// Проверка DO->DI управляется только runtime-параметром debugDisableDeviceSwitchCheck.
// Биты устройств, для которых проверяем подтверждение по входам MB IO.
// Vent1_Left, Vent2_Left, Vent1_Right, Vent2_Right, Ten1_Left, Ten2_Left, Ten1_Right, Ten2_Right, Vent_Out.
static const uint16_t kDeviceCheckMask = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
                                         (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) |
                                         (1u << 8);
// Последний снимок выходного регистра для отслеживания переключений.
static uint16_t g_prevRelayChecked = 0;
static uint8_t g_prevRelayCheckedValid = 0;
// Ожидаемые состояния входов после переключения выходов.
static uint16_t g_pendingCheckMask = 0;
static uint16_t g_expectedDiBits = 0;

// Обработка случая, когда вход не переключился вслед за выходом на следующем опросе.
// TODO: добавить нужную бизнес-логику (авария/лог/уведомление/останов и т.д.).
static void HandleDeviceSwitchCheckMismatch(uint16_t mismatchMask, uint16_t expectedBits, uint16_t actualBits)
{
	// Накопительно фиксируем аварийные биты рассогласования "выход->вход".
	Model::Device_AlarmFlags |= (uint16_t)(mismatchMask & kDeviceCheckMask);
	(void)expectedBits;
	(void)actualBits;
}

// Принудительное отключение ТЭНов, если не подтверждена работа вентиляторов в группе.
static void EnforceHeaterInterlockByFans(void)
{
	const uint8_t leftFansOn = (uint8_t)((Model::DI_DFR.Bits.Vent1_Left != 0) && (Model::DI_DFR.Bits.Vent2_Left != 0));
	const uint8_t rightFansOn = (uint8_t)((Model::DI_DFR.Bits.Vent1_Right != 0) && (Model::DI_DFR.Bits.Vent2_Right != 0));

	if (leftFansOn == 0u)
	{
		Model::DFR.Ten1_Left = 0;
		Model::DFR.Ten2_Left = 0;
		Model::DFR_manual.Ten1_Left = 0;
		Model::DFR_manual.Ten2_Left = 0;
		RelayRegister &= (uint16_t)~((1u << 4) | (1u << 5));
	}

	if (rightFansOn == 0u)
	{
		Model::DFR.Ten1_Right = 0;
		Model::DFR.Ten2_Right = 0;
		Model::DFR_manual.Ten1_Right = 0;
		Model::DFR_manual.Ten2_Right = 0;
		RelayRegister &= (uint16_t)~((1u << 6) | (1u << 7));
	}
}


/************** ДАТЧИКИ ****************************/
/***************************************************/
// проверим все сенсоры на активность, в т.ч. при запуске
void MB_Master_Init(void)
{
	MB_Error_t result = MB_ERROR_NO;
	// Запросим каждый не активный датчик, если ответит - пометим как активный
//	while (1)	{ // тестовый цикл
	for (int i=0; i<SQ; i++)
	{
		if (Sensor_array[i].Active == 0) {
			result = Sensor_Read(i);
			if (result == MB_ERROR_NO) {
				Sensor_array[i].Active = 1; }
			else {
				Sensor_array[i].Active = 0;	}
		}
	}
//	} // тестовый цикл
}
// Функция запускает считывание с датчика в рабочем режиме
MB_Error_t Sensor_Read(uint8_t SensIndex)
{
	MB_Error_t result = MB_ERROR_NO;
	MB_Active_t SW;						// формируем среду работы с датчиками

	// Инициируем среду для работы датчика
	SW.UART = &huart5;		// ← UART5 для датчиков! Master_Request НЕ блокируется для huart5
	SW.PORT = MB_MASTER_DE_GPIO_Port;
	SW.PORT_PIN = MB_MASTER_DE_Pin;
	SW.Sem_Rx = &RX_Compl_SemHandle;
	SW.Sem_Tx = &TX_Compl_SemHandle;
//	/*
//	 *  Датчик другого типа отвечает только после длительной паузы на шине,
//	 *  поэтому после датчика типа 1 надо установить паузу
//	 */
//	if (SW.PreviosTypeOfSensor != Sensor_array[SensIndex].TypeOfSensor) {
//		osDelay(FrameDelay2);	// обеспечение выдержки между фреймами
//	}
	SensAddress = Sensor_array[SensIndex].Address;
	// Считываем данные с датчика определённого типа
	switch (Sensor_array[SensIndex].TypeOfSensor)
	{
	// тип датчика: 1 - совмещённый датчик температуры и влажности GL-TH04-MT
		case 1:		{
			uint8_t REG_COUNT = 2;		// запросим два значения: Н и Т
			// Запросим данные с датчика
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type1_H, REG_COUNT, WR_Buffer);
			break; 	}
	// тип датчика: 2 - датчик температуры РТ100 с RS485
		case 2:		{
			uint8_t REG_COUNT = 1;		// запросим одно значение: Т
			// Запросим данные с датчика
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type2_T, REG_COUNT, WR_Buffer);
			break;	}
	// тип датчика: 4 - модуль ввода-вывода с RS485
		case 4:		{
			/* Запишем состояние регистра аппаратного управления устройствами
			 * в выходной регистр модуля ввода-вывода */
			uint8_t REG_COUNT = 16;		// делаем запись/чтение в/из 16 портов
			WR_Buffer[0] = 2;								// кол-во байт для записи
			WR_Buffer[1] = RelayRegister & 0xFF;			// младший байт
			WR_Buffer[2] = (RelayRegister>>8) & 0xFF;		// старший байт
			result = Master_RW(&SW, SensAddress, MB_CMD_WRITE_COILS, Type4_DO, REG_COUNT, WR_Buffer);
			if (result != MB_ERROR_NO)
				break;
			// Запросим данные с выходного регистра модуля ввода-вывода, запишем в H (Read_Data_1)
			for (uint8_t attempt = 0; attempt < 3; ++attempt)
			{
				result = Master_RW(&SW, SensAddress, MB_CMD_READ_COILS, Type4_DO, REG_COUNT, WR_Buffer);
				if (result == MB_ERROR_NO)
				{
					break;
				}
				// Повторяем только при ошибке приёма.
				if (result != MB_ERROR_UART_RECIEVE && result != MB_ERROR_DMA_RECIEVE)
				{
					break;
				}
			}
			if (result != MB_ERROR_NO)
				break;
			// Запросим данные со входного регистра модуля ввода-вывода, запишем в Т (Read_Data_2)
			for (uint8_t attempt = 0; attempt < 3; ++attempt)
			{
				result = Master_RW(&SW, SensAddress, MB_CMD_READ_INPUT, Type4_DI, REG_COUNT, WR_Buffer);
				if (result == MB_ERROR_NO)
				{
					break;
				}
				// Повторяем только при ошибке приёма.
				if (result != MB_ERROR_UART_RECIEVE && result != MB_ERROR_DMA_RECIEVE)
				{
					break;
				}
			}
			break;	}
		default:	{
			result = MB_ERROR_WRONG_ADDRESS;
			break;	}
	}
	// Если тайминги между разными типами датчиков на шине будут разными, сохраним тип считанного датчика в переменной
	SW.PreviosTypeOfSensor = Sensor_array[SensIndex].TypeOfSensor;
	// Обработка считанных данных
	switch (result)
	{
		case MB_ERROR_NO: {
			Sensor_array[SensIndex].OkCnt++;
			// запись в массив данных
			// Важно: температуры от ModBus приходят 16-битным словом в доп.коде.
			// Для датчиков температуры (типы 1 и 2) нормализуем знак сразу в int16_t,
			// иначе в буферах/усреднении появляются значения около 655xx и скачки +/-3000 °C.
			int tempSigned = SW.Read_Data_2;
			if (Sensor_array[SensIndex].TypeOfSensor == 1 || Sensor_array[SensIndex].TypeOfSensor == 2)
			{
				tempSigned = (int16_t)SW.Read_Data_2;
			}
			Sensor::PutData(TimeFromStart, SensIndex, 1, TimeFromStart);
			Sensor::PutData(TimeFromStart, SensIndex, 2, tempSigned);	// запись Т
			Sensor::PutData(TimeFromStart, SensIndex, 3, SW.Read_Data_1);	// запись Н

			// Почему: состояние входов дефростера нужно обновлять сразу после чтения из модуля ввода-вывода.
			// Модуль ввода-вывода имеет индекс 6 (SQ=7), входы лежат в Read_Data_2 (T).
			if (SensIndex == 6 && Sensor_array[SensIndex].TypeOfSensor == 4)
			{
				Model::DO_DFR.Raw = SW.Read_Data_1;		// Сохраняем считанные с выходов модуля ввода/вывода сигналы устройств
				Model::DI_DFR.Raw = SW.Read_Data_2;		// Сохраняем считанные с входов модуля ввода/вывода сигналы от устройств
				/*********************************************************************************************/
				// Разделяем аварии ворот:
				// - аппаратная авария с входа Gate_Alarm (DI bit 11),
				// - программная авария (таймаут) выставляется в GateControl.
				Model::Gate_Alarm_Hardware = (Model::DI_DFR.Bits.Gate_Alarm != 0) ? 1u : 0u;
				Model::Gate_Alarm = ((Model::Gate_Alarm_Program != 0u) || (Model::Gate_Alarm_Hardware != 0u)) ? 1u : 0u;
				if (Model::Gate_Alarm_Hardware != 0u)
				{
					// Аппаратная авария ворот должна немедленно остановить дефростер.
					DefrostControl_SetEnabled(0);
				}
				/*********************************************************************************************/
			}

			/*****************************************************************************
			 * КОНТРОЛЬ РАбОТОСПОСОбНОСТИ УСТРОЙСТВ
			 *****************************************************************************/
			if (DefrostControl_IsDeviceSwitchCheckEnabled() != 0u)
			{
				// Проверка переключения устройств:
				// сравниваем входы DI с фактическим выходным регистром модуля IO (Read_Data_1),
				// а не с внутренним желаемым состоянием DFR.
				const uint16_t relayNow = (uint16_t)(Model::DO_DFR.Raw & kDeviceCheckMask);
				if (g_prevRelayCheckedValid == 0)
				{
					g_prevRelayChecked = relayNow;	// Начальная установка предыдущих значений в регистре
					g_prevRelayCheckedValid = 1;
				}
				else
				{
					// биты, которые только что поменялись
					const uint16_t relayChanged = (uint16_t)(relayNow ^ g_prevRelayChecked);
					if (relayChanged != 0u)
					{
						// маска для входов, на которых изменилось состояние
						g_pendingCheckMask = (uint16_t)(g_pendingCheckMask | relayChanged);
						// ожидаемые на входах DI сигналы подтверждения включения устройств
						// Для битов, не вошедших в relayChanged, оставляем старое ожидание как было,
						// т.е. должно прийти на входы DI в соответствии с включенными устройствами ранее
						g_expectedDiBits = (uint16_t)((g_expectedDiBits & (uint16_t)(~relayChanged)) 
						// Для битов из relayChanged подставляем новое командное состояние с выходов — relayNow на этих позициях
													| (relayNow & relayChanged));
						// проверенное состояние реле теперь записываем в предыдущее состояние реле
						g_prevRelayChecked = relayNow;
					}
				}

				if (g_pendingCheckMask != 0u)	// если есть изменения битов для проверки
				{
					const uint16_t diNow = (uint16_t)(Model::DI_DFR.Raw & kDeviceCheckMask);	// текущие входы DI
					const uint16_t mismatchMask = (uint16_t)((diNow ^ g_expectedDiBits) & g_pendingCheckMask);	// маска несовпадений
					if (mismatchMask != 0u)	// если есть несовпадения
					{
						// обработка несовпадений
						HandleDeviceSwitchCheckMismatch(mismatchMask, g_expectedDiBits, diNow);
					}
					// Проверка выполняется один раз на "следующем считывании входов" после переключения.
					g_pendingCheckMask = 0u;
				}

				// По входам вентиляторов контролируем межблокировку групп ТЭНов.
				EnforceHeaterInterlockByFans();
			}
			break;
		}
		case MB_ERROR_DMA_SEND:
			Sensor_array[SensIndex].TxErrorCnt++;
			break;
		case MB_ERROR_UART_SEND:
			Sensor_array[SensIndex].TxErrorCnt++;
			break;
		case MB_ERROR_UART_RECIEVE:	// Оба case идут подряд без break, поэтому попадают в один и тот же блок { ... }
		case MB_ERROR_DMA_RECIEVE:	{
			Sensor_array[SensIndex].RxErrorCnt++;
			// Для любой ошибки приёма: в текущий такт сохраняем предыдущее валидное значение.
			// Это важно и для Type=4 (SQ=6): если ошибка была на любом из двух запросов (DO/DI),
			// в буфер попадут прошлые валидные DI/DO без "дыр" и мусора.
			const int prevTemp = Sensor::GetData(TimeFromStart - 1u, SensIndex, 2);	// предыдущее валидное T/DI
			const int prevHum = Sensor::GetData(TimeFromStart - 1u, SensIndex, 3);	// предыдущее валидное H/DO
			Sensor::PutData(TimeFromStart, SensIndex, 1, TimeFromStart);				// запись времени текущего такта
			Sensor::PutData(TimeFromStart, SensIndex, 2, prevTemp);					// запись T/DI
			Sensor::PutData(TimeFromStart, SensIndex, 3, prevHum);					// запись H/DO

			if (SensIndex == 6 && Sensor_array[SensIndex].TypeOfSensor == 4)
			{
				Model::DO_DFR.Raw = (uint16_t)prevHum;
				// Поддерживаем DI_DFR согласованным с тем, что записано в буфер для модуля IO.
				Model::DI_DFR.Raw = (uint16_t)prevTemp;
			}
			break;	}
		case MB_ERROR_COMMAND:
		case MB_ERROR_WRONG_ADDRESS:
		case MB_ERROR_WRONG_VALUE:
			// Неверная команда / адрес / значение в кадре ModBus — как прочие логические ошибки обмена.
			Sensor_array[SensIndex].ErrCnt++;
			result = MB_ERROR_UART_SEND;
			break;
		default:
			Sensor_array[SensIndex].ErrCnt++;
			result = MB_ERROR_UART_SEND;
			break;
	}
	osDelay(FrameDelay1);	// обеспечение выдержки между фреймами
	return result;
}

/**************** ОБРАБОТКА ПРЕРЫВАНИЙ ***************************/
/*****************************************************************/

// обработка прерывания "завершён полный приём", сюда может попасть только если выполнен полный заданный приём
// однако обычно размер пакета неопределён и указывается для приёма большой буфер, который никогда не будет заполнен
// нормальное завершение приёма - это событие, например IDLE, остановка принимаемой информации
// это событие обрабатывается прерыванием HAL_UARTEx_RxEventCallback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart == &huart5)		// приём от датчика
 	{
		// Открыть семафор окончания приёма, продолжится задача ReadData
		osSemaphoreRelease(RX_Compl_SemHandle);
	}
	else if (huart == &huart4)	// сканирование устройства на шине программирования
	{
		osSemaphoreRelease(PR_RX_Compl_SemHandle);
	}
}

// обработка прерывания ошибки
void HAL_UART_ErrorCallback(UART_HandleTypeDef *Uart) {
	HAL_UART_AbortTransmit_IT(Uart);
	if (Uart == &huart5)		// ошибка датчика
	{
		// Включим направление - приём
		HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET);
	}
	else if (Uart == &huart4) 	// ошибка сканирования шины программирования
	{
		HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_RESET);
	}
}

// обработка прерывания приём завершён по событию RX Event callback
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
	if (huart == &huart5)		// приём от датчика
 	{
		// Открыть семафор окончания приёма, продолжится задача ReadData
		osSemaphoreRelease(RX_Compl_SemHandle);
	}
	else if (huart == &huart4)
	{
		if (UART4_IsOwner_Programming() != 0)
		{
			// Во время программирования датчиков UART4 RX-to-IDLE принадлежит ModBus-транзакциям.
			// Приём сервера должен быть выключен, чтобы не конкурировать за DMA-буферы и семафоры.
			osSemaphoreRelease(PR_RX_Compl_SemHandle);
			return;
		}

		// Связь с сервером: сигналим и "RX кадр завершён" (для арбитража half-duplex), и CommandReceiver.
		osSemaphoreRelease(UART4_RX_Event_SemHandle);
		CommandReceiver_OnDataReceived(Size);
	}
}

// обработка прерывания - передача завершена
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *Uart) {
	// Включим направление - приём
	HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET);

	if (Uart == &huart5)
	{
		// Установим семафор окончания передачи, продолжится задача ModBus
		osSemaphoreRelease(TX_Compl_SemHandle);
	}
	else if (Uart == &huart4)
	{
		HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_RESET);
		osSemaphoreRelease(PR_TX_Compl_SemHandle);
	}
}

/***************************************************************************************/

/*Обработчик ошибок*/
uint16_t MB_ErrorHandler(volatile uint8_t * frame, MB_Error_t error) {
    uint16_t txLen = 3;
//    frame[1] |= 0x80;
//    frame[2] = error;
//    uint16_t crc = MB_GetCRC(frame, txLen);
//    frame[txLen++] = crc;
//    frame[txLen++] = crc >> 8;
    return txLen;
}

#define MB_MIN_FRAME_LEN 5


/*Обработка принятого кадра*/
uint16_t MB_TransactionHandler()
{
	uint16_t txLen = 0;
//	/*Проверка длины кадра*/
//	if (len < MB_MIN_FRAME_LEN) return txLen;
//	uint8_t Adderss = MB_Slave_Buffer[0];
//	uint8_t Command = MB_Slave_Buffer[1];
//	uint16_t StartReg = MB_Slave_Buffer[3] | MB_Slave_Buffer[2]<<8;
//	uint16_t RegNum = MB_Slave_Buffer[5] | MB_Slave_Buffer[4]<<8;
//	uint16_t CRC_Sum = MB_Slave_Buffer[6] | MB_Slave_Buffer[7]<<8;
//	if (Adderss != (uint8_t) MB_SLAVE_ADDRESS) return txLen;
//	/*Проверка CRC кадра*/
//	if (CRC_Sum != MB_GetCRC(MB_Slave_Buffer, 6)) return txLen;
//	switch (Command) {
//		case MB_CMD_READ_REGS:
//			txLen = MB_ReadRegsHandler(StartReg, RegNum);
//			break;
////      case MB_CMD_WRITE_REG : txLen = MB_WriteRegHandler(frame, len); break;
////      case MB_CMD_WRITE_REGS : txLen = MB_WriteRegsHandler(frame, len); break;
//		default:
//			txLen = MB_ErrorHandler(MB_Slave_Buffer, MB_ERROR_COMMAND);
//			break;
//	}
	return txLen;
}


/******************* ОБОРУДОВАНИЕ ***********************************/
/********************************************************************/
/*Обработчик команды Read Registers*/
uint16_t MB_ReadRegsHandler(uint16_t StartReg, uint16_t RegNum) {
//	MB_Error_t error = MB_ERROR_NO;
	uint16_t txLen = 0;
//
//	/*Проверка диапазона адресов*/
//	if ((StartReg + RegNum) > MB_SLAVE_REG_COUNT)
//		error = MB_ERROR_WRONG_ADDRESS;
//	/*Проверка максимального количества регистров для чтения*/
//	if (RegNum > 126)
//		error = MB_ERROR_WRONG_VALUE;
//	if (error == MB_ERROR_NO) {
//		uint8_t Bytes = RegNum << 1; /* количество байт */
//		MB_Slave_Buffer[2] = Bytes;
//		/*Копирование данных из памяти в кадр*/
//		memcpy(&MB_Slave_Buffer[3], &(((uint16_t*) &DFR_Reg)[StartReg]), (int) Bytes);
//		txLen = Bytes + 3;
//		/*Расчёт CRC*/
//		uint16_t FrameCRC = MB_GetCRC(MB_Slave_Buffer, txLen);
//		MB_Slave_Buffer[txLen++] = FrameCRC;
//		MB_Slave_Buffer[txLen++] = (FrameCRC >> 8);
//	}
//	else {
//		txLen = MB_ErrorHandler(MB_Slave_Buffer, error);
//	}
	return txLen;
}

/********************** ВСПОМОГАТЕЛЬНЫЕ ******************************/
/*********************************************************************/
uint16_t MB_GetCRC(volatile uint8_t* buf, uint16_t len)
{
	  uint16_t crc_16 = 0xffff;
		for (uint16_t i = 0; i < len; i++)
		{
			crc_16 = (crc_16 >> 8) ^ crc16_table[(buf[i] ^ crc_16) & 0xff];
		}
	  return crc_16;
}

// аппаратное вычисление CRC16
uint16_t CalculateCRC16(uint8_t *buffer, int BufSize) {
uint16_t PolyCRC = 40961;
uint16_t resCRC = 65535;

	for (int i = 0; i < BufSize; i++) {
		resCRC = resCRC ^ buffer[i];
		for (int j=0; j<8; j++) {
			if (resCRC & 1) {
				resCRC = (resCRC>>1) ^ PolyCRC;
			} else {
				resCRC = resCRC>>1;
			}
		}
	}
	return resCRC;
}

/********************** ПРОГРАММИРОВАНИЕ ДАТЧИКОВ ******************************/
/*********************************************************************/
void ProgrammingSensor()
{
/*
 * 	Работаем с тем типом датчика, который установлен в окне программирования
 * 	По-умолчания тип датчика = 0
 */
	uint8_t OldBaudRate = 0;
	uint8_t OldAddress = 0;
	MB_Error_t result = MB_ERROR_NO;
	// объявляем среду работы с датчиками
	MB_Active_t PR;
	// Инициируем среду для программирования датчика
	PR.UART = &huart4;
	PR.PORT = PROG_MASTER_DE_GPIO_Port;
	PR.PORT_PIN = PROG_MASTER_DE_Pin;
	PR.Sem_Rx = &PR_RX_Compl_SemHandle;
	PR.Sem_Tx = &PR_TX_Compl_SemHandle;
	// датчики не искали, выведем на экран инфо об их отсутствии
	Model::setCurrentVal_PR(SensNullValue, SensNullValue);

	while (1)
	{
		switch (Model::Type_of_sensor)
		{
// тип 0 - датчика нет, тип датчика не назначен
			case 0:
				break;
// тип 3 - это датчик температуры Bluetooth
			case 3:
			{

				break;
			}
// по умолчанию - сюда относятся:
			// тип 1 - это датчик совмещенного типа Т и Н GL-TH04-MT
			// тип 2 - это датчик температуры РТ100 с RS485
			// тип 4 - это модуль ввода-вывода
			default:
			{
				// UART4 общий с сервером. В программировании меняется baud и RX DMA-буфер,
				// поэтому серверный RX нужно приостановить на всё время программирования.
				UART4_SetOwner_Programming();
				// Индикация: UART4 в режиме PROGRAMMING (LD4/PG14).
				HAL_GPIO_WritePin(GPIOG, LD4_Pin, GPIO_PIN_SET);
				HAL_UART_AbortReceive(&huart4);
				osDelay(2);

				uint8_t TypeOfSens = Model::Type_of_sensor;
				// цикл будет повторяться, пока оператор не выберет датчик другого типа
				while (Model::Type_of_sensor == TypeOfSens)
				{ // цикл сканирования датчика
					// ═══════════════════════════════════════════════════════════════
					// ЗАХВАТ МЬЮТЕКСА UART4 перед работой с датчиком
					// ═══════════════════════════════════════════════════════════════
					osStatus_t mutexStatus = osMutexAcquire(UART4_MutexHandle, osWaitForever);
					if (mutexStatus != osOK)
					{
						osDelay(10);
						continue;  // Повторяем попытку
					}
					
					result = ScanSensor(&PR);
					OldBaudRate = Model::getCurrentBaudRate_PR();
					OldAddress = Model::getCurrentAddress_PR();
					if (result == MB_ERROR_NO)
					{	//всё хорошо, датчик найден
						// отображение на экране, если новое значение отличается от текущего
						if ((OldBaudRate != SensBaudRateIndex) || (OldAddress != SensPortNumber))
						{
							Model::setCurrentVal_PR(SensPortNumber, SensBaudRateIndex);
						}
					}
					else
					{	//датчик не найден
						Model::setCurrentVal_PR(SensNullValue, SensNullValue);
					}

					osDelay(10); // таймаут
					// если флаг записи (Model::Flag_WR_to_sensor) установлен, выполним запись данных в датчик
					result = WriteToSensor(&PR);
					
					// ═══════════════════════════════════════════════════════════════
					// ОСВОБОЖДЕНИЕ МЬЮТЕКСА UART4 после работы с датчиком
					// ═══════════════════════════════════════════════════════════════
					osMutexRelease(UART4_MutexHandle);

					// Нельзя засыпать, удерживая UART4 mutex, иначе ответы на команды могут задерживаться.
					osDelay(10);

					// Если UI запросил запись, выполняем её отдельной критической секцией.
					if (Model::Flag_WR_to_sensor == 1)
					{
						osStatus_t writeMutexStatus = osMutexAcquire(UART4_MutexHandle, osWaitForever);
						if (writeMutexStatus == osOK)
						{
							result = WriteToSensor(&PR);
							osMutexRelease(UART4_MutexHandle);
						}
					}
				} // конец цикла сканирования и записи в датчик

				// Возвращаем UART4 к настройкам сервера и возобновляем приём команд.
				PR_UART4_Init(19200);
				UART4_SetOwner_Server();
				// Индикация: UART4 вышел из режима PROGRAMMING (LD4/PG14).
				HAL_GPIO_WritePin(GPIOG, LD4_Pin, GPIO_PIN_RESET);
				CommandReceiver_RestartReception();
				break;
			}
		}	// конец оператора switch
	}	// конец бесконечного цикла
}	// конец функции ProgrammingSensor()

/*
 * Функция записывает новые скорость и адрес в датчик, затем считывает записанное и сверяет с заданными
 * Возвращается с результатом записи
 */
MB_Error_t WriteToSensor(MB_Active_t *PR)
{
//	uint8_t i = 0;
	uint8_t WR_BaudRate = 0;
	uint8_t WR_Address = 0;
	MB_Error_t result;

/*
* Запись данных в датчик в режиме программирования датчика, если флаг установлен
* Флаг устанавливается, если датчик найден и выбраны скорость и адрес для записи
*/
	while (Model::Flag_WR_to_sensor == 1)
	{	// начало записи в датчик
		// скорость шины установлена той, на которой датчик работает
		// адрес датчика считан из устройства и записан в SensPortNumber
		// устанавливаем данные для записи нового адреса порта
		WR_Address = Model::Address_WR_to_sensor;
		WR_BaudRate = Model::BaudRate_WR_to_sensor;
		uint8_t REG_COUNT = 2;						// делаем запись в 2 регистра: адрес и скорость
		// заполним буфер для mult записи
		WR_Buffer[0] = 4;							// кол-во байт для записи
		WR_Buffer[1] = (WR_Address>>8) & 0xFF;		// адрес старший байт
		WR_Buffer[2] = WR_Address & 0xFF;			// адрес младший байт
		WR_Buffer[3] = (WR_BaudRate>>8) & 0xFF;		// скорость старший байт
		WR_Buffer[4] = WR_BaudRate & 0xFF;			// скорость младший байт

		// запись адреса и скорости
		switch (Model::Type_of_sensor) {
			case 1:
				result = Master_RW(PR, SensPortNumber, MB_CMD_WRITE_REGS, Type1_Addr, REG_COUNT, WR_Buffer);
				break;
			case 2:
				result = Master_RW(PR, SensPortNumber, MB_CMD_WRITE_REGS, Type2_Addr, REG_COUNT, WR_Buffer);
				break;
			case 4:
				result = Master_RW(PR, SensPortNumber, MB_CMD_WRITE_REGS, Type4_Addr, REG_COUNT, WR_Buffer);
				break;
			default:
				break;
		}

		// проверка записанного
		if (result == MB_ERROR_NO)
		{	//всё хорошо, датчик записан
			// сбрасываем флаг записи в датчик
			Model::Flag_WR_to_sensor = 0;
			// Установим флаг для всплывающего окна предупреждения о необходимости сбросить питание для модуля типа 2 или 4
			if ((Model::Type_of_sensor == 2)||(Model::Type_of_sensor == 4)) {
				Model::Flag_Alert = 1;
			}
		} // конец проверки записанного
		else
			Model::Flag_WR_to_sensor = 0;
		// надо как-то оповестить об ошибке записи
	} // конец записи в датчик
	return result;
}

/* Функция сканирует шину на наличие датчиков по всему разрешённому диапазону скоростей
 * Возвращается с результатом поиска result и значениями в переменных SensPortNumber и SensBaudRateIndex
 * Если датчик был найден, прерывает сканирование и возвращается с результатом
 */
MB_Error_t ScanSensor(MB_Active_t *MB)
{
	MB_Error_t result = MB_ERROR_NO;

	// Производим сканирование широковещательной посылкой шины на всех скоростях
	for (int i = 0; i < BAUD_RATE_NUMBER; ++i)
	{
		switch (Model::Type_of_sensor) {
			case 1: 	{
				PR_UART4_Init(BaudRate_Type1[i]);
				// считываем два регистра: адрес и скорость
				result = Master_RW(MB, 0xFF, MB_CMD_READ_REGS, Type1_Addr, 2, WR_Buffer);
				if (result == MB_ERROR_NO)
				{
					SensPortNumber = MB->Read_Data_1;
					SensBaudRateIndex = MB->Read_Data_2;
				}
				break; 	}
			case 2: 	{
				PR_UART4_Init(BaudRate_Type2[i]);
				// в широковещательном режиме тип 2 позволяет считать только один регистр!
				result = Master_RW(MB, 0xFF, MB_CMD_READ_REGS, Type2_Addr, 1, WR_Buffer);
				if (result == MB_ERROR_NO)
				{
					SensPortNumber = MB->Read_Data_2;
					SensBaudRateIndex = i;
				}
				break; 	}
			case 4: 	{
				PR_UART4_Init(BaudRate_Type4[i]);
				// для этого типа не работает широковещательный адрес, будем перебирать все адреса до ответа устройства
				for (int addr = 1; addr < 4; ++addr)
				{
					// считываем два регистра: адрес и скорость
					result = Master_RW(MB, addr, MB_CMD_READ_REGS, Type4_Addr, 2, WR_Buffer);
					if (result == MB_ERROR_NO)
					{
						SensPortNumber = MB->Read_Data_1;
						SensBaudRateIndex = MB->Read_Data_2;
						break;
					}
					osDelay(10);
				}
				break; 	}
			default:
				break;
		}
		osDelay(10);

		if (result == MB_ERROR_NO) break;
	}
	return result;
}

/* Функция считывает данные с датчика или записывает данные в датчик в зависимости от команды CMD
 * Используется для работы с датчиками для чтения нескольких и записи одного регистра
 * Параметры:
 * - среда работы с датчиком,
 * - адрес датчика,
 * - команда датчику,
 * - начальный регистр,
 * - данные (для чтения - кол-во считываемых регистров, для записи - данные для записи в регистр)
 */
MB_Error_t Master_RW(MB_Active_t *MB, int SensAddress, MB_Command_t CMD, MB_Reg_t START_REG, uint16_t DATA, MultWR_t WR_Buf)
{
	MB_Error_t result;
	memset(MB->Tx_Buffer, 0, MAX_MB_BUFSIZE);
	memset(MB->Rx_Buffer, 0, MAX_MB_BUFSIZE);
	/* в обычной работе для чтения нескольких регистров (команда 0х03)
	 * и записи одного регистра (команда 0х06) требуется 8 байт в посылке */
	int N_Bytes = 8;
	int var, valCRC;

	// параметры для датчика совмещенного типа
	// Выполним приведение типа: указателю Command присвоим указатель буфера, буфер примет тип MB_Frame_t
	MB_Frame_t *Command = (MB_Frame_t*) &MB->Tx_Buffer;
	// Заполним начало буфера структурой для отправки команды датчику
	Command->Address = SensAddress;
	Command->Command = CMD;
	Command->StartReg = SwapBytes(START_REG);
	Command->RegNum = SwapBytes(DATA);
	Command->CRC_Sum = MB_GetCRC(MB->Tx_Buffer, N_Bytes-2);
	// посылка для передачи подготовлена
	// Теперь нужно скорректировать посылку для команд mult записи

	if (CMD >= 0x0F) // это команды mult записи 0x0F, 0x10?
	{
		for (var = 0; var <= WR_Buf[0]; ++var) {
			MB->Tx_Buffer[var + (N_Bytes-2)] = WR_Buf[var];
		}
		// пересчитаем CRC
		valCRC = MB_GetCRC(MB->Tx_Buffer, (N_Bytes-2) + var);
		MB->Tx_Buffer[var++ + (N_Bytes-2)] = valCRC & 0xFF;			// CRC Lo
		MB->Tx_Buffer[var++ + (N_Bytes-2)] = valCRC>>8 & 0xFF;		// CRC Hi
		result = Master_Request(MB, var + (N_Bytes-2));
	}
	else
		result = Master_Request(MB, N_Bytes);

	if (result == MB_ERROR_NO)
	{
		// данные приняты - проверяем достоверность и сохраняем принятые данные в переменные
		switch (CMD)	{
			case MB_CMD_READ_COILS:	{
				if (CMD == *(uint8_t*) &MB->Rx_Buffer[1])
					// читаем два байта
					MB->Read_Data_1 = *(uint16_t*) &MB->Rx_Buffer[3];
				else	{
					// возможно, была ошибка. Код ошибки сохраним в Read_Data_1
					MB->Read_Data_1 = *(uint16_t*) &MB->Rx_Buffer[2];
					result = MB_ERROR_UART_SEND;	}
				break;	}
			case MB_CMD_READ_INPUT:	{
				if (CMD == *(uint8_t*) &MB->Rx_Buffer[1])
					// читаем два байта
					MB->Read_Data_2 = *(uint16_t*) &MB->Rx_Buffer[3];
				else	{
					// возможно, была ошибка. Код ошибки сохраним в Read_Data_2
					MB->Read_Data_2 = *(uint16_t*) &MB->Rx_Buffer[2];
					result = MB_ERROR_UART_SEND;	}
				break;	}
			case MB_CMD_READ_REGS: {	// был запрос на чтение одного или нескольких регистров
				if CheckAnswerCRC
				{	// все проверки ОК
					if (DATA == 1) // заказывали один регистр на чтение
					{// все проверки ОК, читаем одно значение
						MB->Read_Data_1 = 0;
						MB->Read_Data_2 = SwapBytes( *(uint16_t*) &MB->Rx_Buffer[3]);
					}
					else
					{// все проверки ОК, читаем два значения
						MB->Read_Data_1 = SwapBytes( *(uint16_t*) &MB->Rx_Buffer[3]);
						MB->Read_Data_2 = SwapBytes( *(uint16_t*) &MB->Rx_Buffer[5]);
					}
				}
				else
				{	// проверки не пройдены, ошибка в принятых данных
					result = MB_ERROR_UART_SEND;
				};
				break;	}
			case MB_CMD_WRITE_REG: {	// был запрос на запись одного регистра
				if PR_CheckAnswerCRC
				{	// Считанные из датчика данные после записи помещаем в переменную
					Sens_WR_value = SwapBytes( *(uint16_t*) &MB->Rx_Buffer[4]);
				}
				else
				{	// проверки не пройдены, ошибка в принятых данных
					result = MB_ERROR_UART_SEND;
				};
				break;	}
			case MB_CMD_WRITE_REGS: {	// был запрос на запись нескольких регистров
				break;	}
			default:	{	//
				break;	}
		}
	}
	return result;
}

// запрос датчикам на шине ModBus
/*
MB_ERROR_NO = 0x00,
MB_ERROR_COMMAND = 0x01,
MB_ERROR_WRONG_ADDRESS = 0x02,
MB_ERROR_WRONG_VALUE = 0x03,
MB_ERROR_DMA_SEND = 0x04,
MB_ERROR_UART_SEND = 0x05,
MB_ERROR_UART_RECIEVE = 0x06,
MB_ERROR_DMA_RECIEVE = 0x07
*/
/*
 * Функция: CheckAndWaitForActiveReception
 * Описание: Проверяет, идёт ли активный приём данных по UART, и ждёт его завершения
 * Параметры:
 *   - uart: указатель на UART handle
 *   - sem_rx: указатель на семафор приёма
 * Возвращает:
 *   - MB_ERROR_NO: приём завершён или не был активен, можно передавать
 *   - MB_ERROR_UART_SEND: не удалось дождаться завершения приёма
 * 
 * ВАЖНО: Эта функция критична для RS-485 (Half Duplex)!
 * Она различает два состояния BUSY_RX:
 *   1. Режим ожидания (IDLE) - можно прервать
 *   2. Активный приём данных - НУЖНО дождаться завершения!
 */
MB_Error_t CheckAndWaitForActiveReception(UART_HandleTypeDef *uart, osSemaphoreId_t *sem_rx)
{
	HAL_UART_StateTypeDef uartState = HAL_UART_GetState(uart);
	
	// Проверяем RX канал - КРИТИЧНО для RS-485!
	// RS-485 = Half Duplex: если идёт приём, НЕЛЬЗЯ начинать передачу
	if ((uartState & HAL_UART_STATE_BUSY_RX) == HAL_UART_STATE_BUSY_RX)
	{
		// Проверяем, идёт ли АКТИВНЫЙ приём данных (не просто режим ожидания)
		// Считываем счётчик DMA дважды с небольшой задержкой
		uint32_t dmaCounter1 = __HAL_DMA_GET_COUNTER(uart->hdmarx);
		osDelay(1);  // Минимальная задержка для детекта активного RX без заметного влияния на задержку ответа
		uint32_t dmaCounter2 = __HAL_DMA_GET_COUNTER(uart->hdmarx);
		
		// Если счётчик изменился - данные РЕАЛЬНО принимаются прямо сейчас!
		if (dmaCounter1 != dmaCounter2)
		{
			// АКТИВНЫЙ ПРИЁМ ДАННЫХ!
			// Ждём семафор завершения приёма (его выдаст прерывание HAL_UARTEx_RxEventCallback)
			// Таймаут 100 мс - достаточно для приёма любого пакета
			osStatus_t semStatus = osSemaphoreAcquire(*sem_rx, 100);
			
			// Если не дождались семафора - приём завис
			if (semStatus != osOK)
			{
				// КРИТИЧНО: Лучше отменить передачу, чем потерять приём!
				return MB_ERROR_UART_SEND;
			}
		}
		else
		{
			// Счётчик не меняется = просто режим ожидания (IDLE)
			// Можно безопасно прервать ожидание и начать передачу
			HAL_UART_AbortReceive(uart);
			osDelay(1);
		}
	}
	
	return MB_ERROR_NO;
}

static void DrainBinarySemaphore(osSemaphoreId_t sem)
{
	// Гарантируем, что следующее ожидание увидит новое событие, а не "залежавшийся" токен.
	while (osSemaphoreAcquire(sem, 0) == osOK)
	{
	}
}

static MB_Error_t WaitUntilUART4RxFrameCompletes(void)
{
	// Хелпер актуален только для UART4 RX-to-IDLE DMA режима, используемого со связью с сервером.
	HAL_UART_StateTypeDef uartState = HAL_UART_GetState(&huart4);
	if ((uartState & HAL_UART_STATE_BUSY_RX) != HAL_UART_STATE_BUSY_RX)
	{
		return MB_ERROR_NO;
	}

	// Определяем, идут ли байты прямо сейчас или UART просто "висит" в режиме ожидания RX.
	uint32_t dmaCounter1 = __HAL_DMA_GET_COUNTER(huart4.hdmarx);
	osDelay(1);
	uint32_t dmaCounter2 = __HAL_DMA_GET_COUNTER(huart4.hdmarx);

	if (dmaCounter1 != dmaCounter2)
	{
		DrainBinarySemaphore(UART4_RX_Event_SemHandle);
		osStatus_t semStatus = osSemaphoreAcquire(UART4_RX_Event_SemHandle, 100);
		return (semStatus == osOK) ? MB_ERROR_NO : MB_ERROR_UART_SEND;
	}
	return MB_ERROR_NO;
}

static uint8_t UART4_IsReceivingBytesNow(void)
{
	HAL_UART_StateTypeDef uartState = HAL_UART_GetState(&huart4);
	if ((uartState & HAL_UART_STATE_BUSY_RX) != HAL_UART_STATE_BUSY_RX)
	{
		return 0;
	}

	uint32_t dmaCounter1 = __HAL_DMA_GET_COUNTER(huart4.hdmarx);
	osDelay(1);
	uint32_t dmaCounter2 = __HAL_DMA_GET_COUNTER(huart4.hdmarx);
	return (dmaCounter1 != dmaCounter2) ? 1 : 0;
}

/* Функция посылает запрос датчику и принимает ответ.
 * Параметры порта uart, GPIO, буферы и семафоры передачи и приёма передаются в структуре MB.
 * Возвращается статус обработки запроса к датчику.
 */
MB_Error_t Master_Request(MB_Active_t *MB, int N_Bytes)
{
	MB_Error_t MB_ERR = MB_ERROR_NO;
	HAL_StatusTypeDef result;		// статус HAL: HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT
	// Рассчитываем таймаут примерно на 1000 бит для текущего baud.
	// Важно: переполнение uint8_t на низких скоростях укорачивает ожидание и даёт ложные таймауты.
	const uint32_t baud = (uint32_t)MB->UART->Init.BaudRate;
	const uint32_t pauseMs = (baud == 0u) ? 100u : ((1000000u + baud - 1u) / baud);

	// Гарантируем, что каждая транзакция ждёт новое событие завершения.
	// "Залежавшийся" токен приведёт к мгновенному выходу из ожидания и разбору пустого/частичного буфера.
	DrainBinarySemaphore(*MB->Sem_Tx);
	DrainBinarySemaphore(*MB->Sem_Rx);

	// ═══════════════════════════════════════════════════════════════════════════
	// ВАЖНО! Эта проверка выполняется ТОЛЬКО для UART4 (связь с сервером)
	// Для UART5 (датчики) эта проверка НЕ выполняется - датчики работают независимо!
	// ═══════════════════════════════════════════════════════════════════════════
	// RS-485 сервер использует Half Duplex - один физический канал для приёма И передачи.
	// Даже если UART полнодуплексный, RS-485 НЕ МОЖЕТ принимать и передавать одновременно.
	if (MB->UART == &huart4)  // ← Проверка ТОЛЬКО для сервера!
	{
		HAL_UART_StateTypeDef uartState = HAL_UART_GetState(MB->UART);
		
		// Проверяем TX канал - если занят, ЖДЁМ завершения передачи через семафор
		if ((uartState & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX)
		{
			// Ждём семафор завершения передачи (его выдаст прерывание HAL_UART_TxCpltCallback)
			// Таймаут 100 мс - достаточно для любой передачи
			osStatus_t semStatus = osSemaphoreAcquire(*MB->Sem_Tx, 100);
			
			// Если не дождались семафора - передача зависла, возвращаем ошибку
			if (semStatus != osOK)
			{
				return MB_ERROR_UART_SEND;
			}
		}
		
		// Проверяем RX канал и ждём завершения активного приёма (если он идёт)
		MB_ERR = CheckAndWaitForActiveReception(MB->UART, MB->Sem_Rx);
		if (MB_ERR != MB_ERROR_NO)
		{
			return MB_ERR;  // Не удалось дождаться завершения приёма
		}
		
		// Финальная проверка: TX канал должен быть свободен
		if ((HAL_UART_GetState(MB->UART) & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX)
		{
			return MB_ERROR_UART_SEND;
		}
	}

	// ПЕРЕДАЧА DMA ********************************
	// Включим направление - передача
	HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_SET);
	// Начинаем передачу отправкой буфера с записанной структурой в порт UART через DMA
	osDelay(1);	// задержка перед стартовым битом

	result = HAL_UART_Transmit_DMA(MB->UART, MB->Tx_Buffer, N_Bytes);
	if (result == HAL_OK)
	{
		// ПЕРЕДАЧА UART ***************************
		// Ждём, пока UART всё передаст в шину и обработчик прерывания HAL_UART_TxCpltCallback выдаст токен семафора
		resultSem = osSemaphoreAcquire(*MB->Sem_Tx, pauseMs);
		if (resultSem != osOK)
		{	// обработка ошибки передачи по UART
			MB_ERR = MB_ERROR_UART_SEND;
			HAL_UART_AbortTransmit_IT(MB->UART);
			// Включим направление - приём
			HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_RESET);
			return MB_ERR;
		}
		// Направление на приём включается в обработчике прерывания HAL_UART_TxCpltCallback

		// ПРИЁМ DMA *******************************
		// Функция принимает объем данных в режиме DMA до тех пор,
		// пока не будет получено ожидаемое количество данных или не произойдет событие ПРОСТОЯ.
		result = HAL_UARTEx_ReceiveToIdle_DMA(MB->UART, MB->Rx_Buffer, MAX_MB_BUFSIZE);
	    // Отключаем прерывание половины приёма
	    __HAL_DMA_DISABLE_IT(MB->UART->hdmarx, DMA_IT_HT);

		if (result == HAL_OK)
		{	// ReceiveToIdle_DMA отработал и вышел по тайм-ауту
			// последнее значение в очереди = 0, ждём прерывание приёма по IDLE
			// Ждём, когда приём закончится и прерывание выдаст токен семафора
			//ответ должен нормально уложиться в 11 байт (1200 -> 9.1 ms на байт, всего на фрейм 72,8 ms), это время функция ждёт токен семафора в состоянии блокировки
		resultSem = osSemaphoreAcquire(*MB->Sem_Rx, pauseMs);
		if (resultSem != osOK)
			{	// прерывания не случилось, семафора не дождались, вышли по тайм-ауту
				MB_ERR = MB_ERROR_UART_RECIEVE;
				// датчик не ответил, прекращаем ReceiveToIdle_DMA
				HAL_UART_AbortReceive_IT(MB->UART);
				return MB_ERR;
			}
		}
		else
		{  // обработка ошибки приёма
			MB_ERR = MB_ERROR_DMA_RECIEVE;
		}
	}
	else
	{  // обработка ошибки передачи по DMA
		MB_ERR = MB_ERROR_DMA_SEND;
		HAL_UART_AbortTransmit_IT(MB->UART);
		// Включим направление - приём
		HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_RESET);
	}
	return MB_ERR;
}

/* Функция устанавливает скорость обмена порта uart4.
 * Применяется при сканировании шины на наличие датчиков с помощью широковещательного запроса.
 */
void PR_UART4_Init(int BaudRateValue)
{
  huart4.Init.BaudRate = BaudRateValue;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
}

/*
 * ******************  КОРРЕКТИРОВКА **************************
 */
// Функция считывает параметры с датчика
MB_Error_t Sensor_Read_CORR(uint8_t SensIndex)
{
	MB_Error_t result = MB_ERROR_NO;
	MB_Active_t SW;						// объявляем среду работы с датчиками


	// Инициируем среду для работы датчика
	SW.UART = &huart5;
	SW.PORT = MB_MASTER_DE_GPIO_Port;
	SW.PORT_PIN = MB_MASTER_DE_Pin;
	SW.Sem_Rx = &RX_Compl_SemHandle;
	SW.Sem_Tx = &TX_Compl_SemHandle;
//	// Считываем данные с датчика определённого типа
//	/*
//	 *  Датчик другого типа отвечает только после длительной паузы на шине,
//	 *  поэтому после датчика типа 1 надо установить паузу
//	 */
//	if (SW.PreviosTypeOfSensor != Sensor_array[SensIndex].TypeOfSensor) {
//		osDelay(FrameDelay2);	// обеспечение выдержки между фреймами
//	}
	SensAddress = Sensor_array[SensIndex].Address;
	switch (Sensor_array[SensIndex].TypeOfSensor)
	{
	// тип датчика: 1 - совмещённый датчик температуры и влажности GL-TH04-MT
		case 1:		{
			// Запросим данные с датчика
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type1_H, 2, WR_Buffer);
			Model::H_CORR_sensor = SW.Read_Data_1;
			// Температура в two's complement (deci °C): нормализуем знак.
			Model::T_CORR_sensor = (int16_t)SW.Read_Data_2;
			break; 	}
	// тип датчика: 2 - датчик температуры РТ100 с RS485
		case 2:		{
			// Запросим данные с датчика
			// Одно значение получаем всегда в Read_Data_2
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type2_R, 1, WR_Buffer);
			Model::R_CORR_sensor = SW.Read_Data_2;
			// Здесь паузу между фреймами не делаем, поскольку читаем из того же устройства
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type2_T, 1, WR_Buffer);
			// Температура в two's complement (deci °C): нормализуем знак.
			Model::T_CORR_sensor = (int16_t)SW.Read_Data_2;
			break;	}
		default:	{
			result = MB_ERROR_WRONG_ADDRESS;
			break;	}
	}
	SW.PreviosTypeOfSensor = Sensor_array[SensIndex].TypeOfSensor;
	osDelay(FrameDelay1);	// обеспечение выдержки между фреймами
	return result;
}

// Функция записывает в датчик
MB_Error_t Sensor_Write_CORR(uint8_t SensIndex)
{
	MB_Error_t result = MB_ERROR_COMMAND;
	Model::Flag_WR_to_sensor = 0;
	MB_Active_t SW;						// объявляем среду работы с датчиками


	int CORR_Data;
	// Инициируем среду для работы датчика
	SW.UART = &huart5;
	SW.PORT = MB_MASTER_DE_GPIO_Port;
	SW.PORT_PIN = MB_MASTER_DE_Pin;
	SW.Sem_Rx = &RX_Compl_SemHandle;
	SW.Sem_Tx = &TX_Compl_SemHandle;
	// Записываем данные в датчик определённого типа
	// Пишем T
	if (Model::CORR_T_sensor != 0) {
		SensAddress = Sensor_array[SensIndex].Address;
		switch (Sensor_array[SensIndex].TypeOfSensor)
		{
		// тип датчика: 1 - совмещённый датчик температуры и влажности GL-TH04-MT
			case 1:		{	// Пишем Т
				// Считаем имеющуюся корректировку
				result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type1_T_calibr, 1, WR_Buffer);
				// вычисляем разность величин с учётом имеющейся корректировки
				CORR_Data = Model::CORR_T_sensor - (Model::T_CORR_sensor - SW.Read_Data_2);
				result = Master_RW(&SW, SensAddress, MB_CMD_WRITE_REG, Type1_T_calibr, CORR_Data, WR_Buffer);
				break; 	}
		// тип датчика: 2 - датчик температуры РТ100 с RS485
			case 2:		{	// Пишем T
				CORR_Data = Model::CORR_T_sensor;								// пишем нужную величину
				result = Master_RW(&SW, SensAddress, MB_CMD_WRITE_REG, Type2_T_calibr, CORR_Data, WR_Buffer);
				break;	}
			default:	{
				result = MB_ERROR_WRONG_ADDRESS;
				break;	}
		}
	};
	// Пишем HR
	osDelay(3);	// обеспечение выдержки между фреймами >3.5 времени передачи байта
	SensAddress = Sensor_array[SensIndex].Address;
	switch (Sensor_array[SensIndex].TypeOfSensor)
	{
	// тип датчика: 1 - совмещённый датчик температуры и влажности GL-TH04-MT
		case 1:		{	// Пишем H
			if (Model::CORR_H_sensor != 0)
			{
				// Считаем имеющуюся корректировку
				result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type1_H_calibr, 1, WR_Buffer);
				// вычисляем разность величин с учётом имеющейся корректировки
				CORR_Data = Model::CORR_H_sensor - (Model::H_CORR_sensor - SW.Read_Data_2);
				result = Master_RW(&SW, SensAddress, MB_CMD_WRITE_REG, Type1_H_calibr, CORR_Data, WR_Buffer);
			}
			break; 	}
	// тип датчика: 2 - датчик температуры РТ100 с RS485
		case 2:		{	// Пишем R
			if (Model::CORR_R_sensor != 0)
			{
				CORR_Data = Model::CORR_R_sensor;								// пишем нужную величину
				result = Master_RW(&SW, SensAddress, MB_CMD_WRITE_REG, Type2_R_calibr, CORR_Data, WR_Buffer);
			}
			break;	}
		default:	{
			result = MB_ERROR_WRONG_ADDRESS;
			break;	}
	}
	return result;
}

// Функция обнуляет корректировки в датчике
MB_Error_t Sensor_CORR_Reset(uint8_t SensIndex)
{
	MB_Error_t result = MB_ERROR_COMMAND;
	Model::Flag_Alert = 0;
	MB_Active_t SW;						// объявляем среду работы с датчиками


	// Инициируем среду для работы датчика
	SW.UART = &huart5;
	SW.PORT = MB_MASTER_DE_GPIO_Port;
	SW.PORT_PIN = MB_MASTER_DE_Pin;
	SW.Sem_Rx = &RX_Compl_SemHandle;
	SW.Sem_Tx = &TX_Compl_SemHandle;

	// Обнуляем корректировку Т
	SensAddress = Sensor_array[SensIndex].Address;
	switch (Sensor_array[SensIndex].TypeOfSensor)
	{
	// тип датчика: 1 - совмещённый датчик температуры и влажности GL-TH04-MT
		case 1:		{
			// Обнуляем корректировку Т
			result = Master_RW(&SW, SensAddress, MB_CMD_WRITE_REG, Type1_T_calibr, 0, WR_Buffer);
			// Обнуляем корректировку Н
			result = Master_RW(&SW, SensAddress, MB_CMD_WRITE_REG, Type1_H_calibr, 0, WR_Buffer);
			break; 	}
	// тип датчика: 2 - датчик температуры РТ100 с RS485
		case 2:		{
			break;	}
		default:	{
			result = MB_ERROR_WRONG_ADDRESS;
			break;	}
	}
	return result;
}

/*
 * Функция: Master_SendTelemetry
 * Описание: Отправка телеметрии на сервер БЕЗ ожидания ответа
 * 
 * КРИТИЧНО: Эта функция НЕ ЖДЁТ ответа от сервера!
 * Ответ будет обработан асинхронно через CommandReceiver_Task
 * 
 * Параметры:
 *   - MB: указатель на структуру среды работы с шиной
 *   - N_Bytes: количество байт для отправки
 * 
 * Возвращает: MB_Error_t - результат операции передачи
 * 
 * ОТЛИЧИЕ ОТ Master_Request:
 *   Master_Request: отправка → ожидание ответа → возврат
 *   Master_SendTelemetry: отправка → возврат (ответ обработает CommandReceiver)
 */
MB_Error_t Master_SendTelemetry(MB_Active_t *MB, int N_Bytes)
{
	MB_Error_t MB_ERR = MB_ERROR_NO;
	HAL_StatusTypeDef result;
	osStatus_t resultSem;
	
	// Вычислим паузу для ожидания завершения передачи
	double var = (1000 * 1000) / MB->UART->Init.BaudRate;
	uint8_t pause = uint8_t (var);

	// ═══════════════════════════════════════════════════════════════════════════
	// Проверки для UART4 (RS-485 Half Duplex с сервером)
	// ═══════════════════════════════════════════════════════════════════════════
	if (MB->UART == &huart4)
	{
		HAL_UART_StateTypeDef uartState = HAL_UART_GetState(MB->UART);
		
		// Проверяем TX канал - если занят, ЖДЁМ завершения передачи
		if ((uartState & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX)
		{
			osStatus_t semStatus = osSemaphoreAcquire(*MB->Sem_Tx, 100);
			if (semStatus != osOK)
			{
				return MB_ERROR_UART_SEND;
			}
		}
		
		// Проверяем RX канал и ждём завершения активного приёма (если он идёт)
		MB_ERR = CheckAndWaitForActiveReception(MB->UART, MB->Sem_Rx);
		if (MB_ERR != MB_ERROR_NO)
		{
			return MB_ERR;
		}
		
		// Финальная проверка: TX канал должен быть свободен
		if ((HAL_UART_GetState(MB->UART) & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX)
		{
			return MB_ERROR_UART_SEND;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// ПЕРЕДАЧА ДАННЫХ
	// ═══════════════════════════════════════════════════════════════════════════
	// Включим направление - передача
	HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_SET);
	osDelay(1);	// задержка перед стартовым битом

	result = HAL_UART_Transmit_DMA(MB->UART, MB->Tx_Buffer, N_Bytes);
	if (result == HAL_OK)
	{
		// Ждём завершения передачи
		resultSem = osSemaphoreAcquire(*MB->Sem_Tx, pause/portTICK_RATE_MS);
		if (resultSem != osOK)
		{
			MB_ERR = MB_ERROR_UART_SEND;
			HAL_UART_AbortTransmit_IT(MB->UART);
			// Включим направление - приём
			HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_RESET);
			return MB_ERR;
		}
		// Направление на приём включается в обработчике прерывания HAL_UART_TxCpltCallback
		
		// ═══════════════════════════════════════════════════════════════════════════
		// КРИТИЧНО: НЕ ЗАПУСКАЕМ ПРИЁМ ЗДЕСЬ!
		// Приём уже работает в CommandReceiver через HAL_UARTEx_ReceiveToIdle_DMA
		// Ответ от сервера будет обработан асинхронно в CommandReceiver_Task
		// ═══════════════════════════════════════════════════════════════════════════
	}
	else
	{
		MB_ERR = MB_ERROR_DMA_SEND;
		HAL_UART_AbortTransmit_IT(MB->UART);
		// Включим направление - приём
		HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_RESET);
	}
	
	return MB_ERR;
}

// Функция для предотвращения конфликта приёма и передачи данных на сервер и для захвата мьютекса UART4
static MB_Error_t UART4_TryEnterForTx(void)
{
	MB_Error_t result;

	// Если прямо сейчас приходит кадр от сервера, ждём его завершения, не удерживая мьютекс UART4.
	result = WaitUntilUART4RxFrameCompletes();
	if (result != MB_ERROR_NO)
	{
		return result;
	}

	// Захват мьютекса UART4 — защита от конфликта с программированием датчиков
	osStatus_t mutexStatus = osMutexAcquire(UART4_MutexHandle, osWaitForever);
	if (mutexStatus != osOK)
	{
		// Не удалось захватить мьютекс — считаем это ошибкой шины
		return MB_ERROR_UART_SEND;
	}

	// Повторная проверка после захвата: если CommandReceiver начал обработку, сразу уступаем.
	if (CommandReceiver_IsHandling() != 0)
	{
		osMutexRelease(UART4_MutexHandle);
		osDelay(1);
		return MB_ERROR_UART_SEND;
	}

	// Если байты приходят прямо сейчас, не удерживаем мьютекс во время ожидания.
	if (UART4_IsReceivingBytesNow() != 0)
	{
		osMutexRelease(UART4_MutexHandle);
		osDelay(1);
		return MB_ERROR_UART_SEND;
	}

	// Перед переключением направления RS-485 в TX гарантируем остановку RX DMA.
	HAL_UART_AbortReceive(&huart4);
	osDelay(2);

	// На этом шаге:
	// - RX кадр не идёт,
	// - CommandReceiver не обрабатывает команду,
	// - мьютекс UART4 захвачен вызывающим потоком,
	// - RX остановлен, можно переключать в TX и отправлять.
	return MB_ERROR_NO;
}

// Функция для передачи данных на сервер
void WriteToServer(uint8_t* Data, int length)
{
	MB_Error_t result;
	MB_Active_t MB;						// объявляем среду работы с шиной

	// Если UART4 занят программированием, не отправляем данные на сервер.
	if (UART4_IsOwner_Programming() != 0)
	{
		return;
	}

	// Ждём завершения обработки данных (команд) от сервера
	while (CommandReceiver_IsHandling() != 0)
	{
		osDelay(1);
	}

	// Пытаемся захватить мьютекс UART4 для передачи данных на сервер
	for (;;)
	{
		result = UART4_TryEnterForTx();
		if (result != MB_ERROR_NO)
		{
		// Освобождение мьютекса при ошибке уже сделано внутри
		osDelay(1);
		continue;
		}
	
		break;
	}

	// Инициируем среду для работы по шине программирования
	MB.UART = &huart4;		// ← UART4 для сервера
	MB.PORT = PROG_MASTER_DE_GPIO_Port;
	MB.PORT_PIN = PROG_MASTER_DE_Pin;
	MB.Sem_Rx = &UART4_RX_Event_SemHandle;
	MB.Sem_Tx = &PR_TX_Compl_SemHandle;
	
	// Копируем данные в буфер передачи
    memcpy(MB.Tx_Buffer, Data, length);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ИСПОЛЬЗУЕМ Master_SendTelemetry вместо Master_Request!
    // Это позволяет НЕ ЖДАТЬ ответа - ответ обработает CommandReceiver
    // ═══════════════════════════════════════════════════════════════════════════
	result = Master_SendTelemetry(&MB, length);
	
	// После передачи перезапускаем приём команд от сервера
	// ВАЖНО для RS-485 (Half Duplex): убеждаемся что приём активен
	CommandReceiver_RestartReception();
	
	// ═══════════════════════════════════════════════════════════════════════════
	// ОСВОБОЖДЕНИЕ МЬЮТЕКСА UART4
	// ═══════════════════════════════════════════════════════════════════════════
	osMutexRelease(UART4_MutexHandle);
	
	if (result != MB_ERROR_NO){
		// Обработка ошибки передачи (можно добавить логирование)
	}
}

/*
 * Функция: WriteToServerWithSync
 * Описание: Отправка пакета на сервер с маркером начала (AA 55), но без маркера конца
 * 
 * ФОРМАТ ПАКЕТА:
 *   [AA 55] [Type + Code + Status + dataLen + data + CRC16]
 *   └──┬──┘ └────────────────────┬────────────────────────┘
 *    START            	         Data
 * 
 * Параметры:
 * 	 - START  		маркер начала пакета
 *   - Data		   	указатель на пакет с данными
 *   - Type			тип команды
 *   - Code			команда
 *   - Status		статус исполнения команды
 *   - dataLen 	  	длина данных data
 *   - data			данные data
 *   - CRC16		CRC по пакету Data
 */
void WriteToServerWithSync(uint8_t* Data, int length)
{
	static uint8_t TxBufferWithSync[MAX_MB_BUFSIZE];

	TxBufferWithSync[0] = SYNC_START_1;  // 0xAA
	TxBufferWithSync[1] = SYNC_START_2;  // 0x55
	
	// Добавляем в буфер данные (Type + Len + PayLoad + CRC)
	memcpy(&TxBufferWithSync[2], Data, length);
	
	WriteToServer(TxBufferWithSync, length + 2);	// Отправляем пакет на сервер
}
