#include "main.h"
#include "cmsis_os.h"
#include "ModBus.hpp"
#include "Data.hpp"
#include "stdio.h"
#include "task.h"
#include "string.h"
#include "queue.h"
#include <gui\model\model.hpp>

//#include "usb_host.h"

#define  MAX_MB_BUFSIZE 40

extern UART_HandleTypeDef huart4;				// для программирования датчиков
extern UART_HandleTypeDef huart5;				// для считывания данных с датчиков
extern osSemaphoreId_t TX_Compl_SemHandle;		// семафор окончания передачи датчикам
extern osSemaphoreId_t RX_Compl_SemHandle;		// семафор окончания приёма от датчиков
extern osSemaphoreId_t PR_TX_Compl_SemHandle;	// семафор окончания приёма при программировании
extern osSemaphoreId_t PR_RX_Compl_SemHandle;	// семафор окончания передачи при программировании

extern unsigned int TimeFromStart;
extern uint16_t RelayRegister;	// временная переменная, заменяющая регистр аппаратного управления устройствами
												// volatile - может изменяться другими потоками

uint8_t MB_MasterTx_Buffer[MAX_MB_BUFSIZE] = {0};
uint8_t MB_MasterRx_Buffer[MAX_MB_BUFSIZE] = {0};
using MultWR_t = int8_t[8];						// Тип данных - Буфер для данных mult команд записи в устройство ModBus
MultWR_t WR_Buffer = {0};						// Обявление буфера для mult команд

uint16_t master_rec_byte_count = 0;
uint16_t CountRX = 0;
uint8_t FrameDelay1 = 30;						// Задержка между фреймами рабочая
//uint8_t FrameDelay2 = 0;						// Задержка между фреймами дополнительная при смене типа датчика
/*
 * Задержка между фреймами подобрана опытным путём. По стандарту ModBus RTU "Перед началом передачи очередного фрейма, необходима выдержка времени,
 * соответствующая 3,5 временам передачи одного байта данных (t3,5) после завершения передачи предыдущего фрейма (или “ложной” передачи данных)."
 * Однако на практике датчики начали отвечать без сбоев только при удержании шины в свободном состоянии в течение 30 мс - это примерно 6 фреймов
 * (не байт, а фреймов) на скорости 19200.
*/

uint16_t MB_TransactionHandler();
osStatus_t resultSem;			/* status семафора:  	osOK: токен получен, и количество токенов уменьшено.
														osErrorTimeout: не удалось получить токен в заданное время.
														osErrorResource: не удалось получить токен, если не был указан тайм-аут.
														osErrorParameter: параметр semaphore_id имеет значение NULL или недопустим */

/*CRC16-CITT tables*/
const static uint16_t crc16_table[] =
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
		{3, "BT T"},			// 3 - датчик температуры BlueTooth
		{4, "MB IO"}			// 4 - модуль ввода-вывода MB 16DI-16RO
};

SENSOR_typedef_t Sensor_array[SQ] =
{
		{101,3,0,1,"Left def", 0,0,0,0},		// 0 - defroster left, 	GL-TH04-MT
		{102,3,0,1,"Right def",0,0,0,0},		// 1 - defroster right,	GL-TH04-MT
		{103,3,0,1,"Center def",0,0,0,0},		// 2 - defroster center,GL-TH04-MT
		{104,3,0,2,"Left prod",0,0,0,0},		// 3 - fish left, 		РТ100 с RS485
		{105,3,0,2,"Right prod",0,0,0,0},		// 4 - fish right,		РТ100 с RS485
		{002,3,0,4,"MB 16IO",0,0,0,0}			// 5 - модуль ввода-вывода с RS485, диапазон адресов: 2 и 3
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
MB_Error_t Master_RW(MB_Active_t *MB, int Address, MB_Command_t CMD, MB_Reg_t START_REG, uint16_t DATA, MultWR_t WR_Buf);
//MB_Error_t Master_Read(MB_Active_t *MB, uint8_t SensIndex, MB_Command_t CMD, MB_Reg_t START_REG, uint8_t DATA);
MB_Error_t ScanSensor(MB_Active_t *MB);
MB_Error_t WriteToSensor(MB_Active_t *PR);
// при чтении из датчика значение кол-ва переданных байт данных в Rx_Buffer[2] + всегда передаётся 5 байт
#define CheckAnswerCRC (MB->Rx_Buffer[1] == CMD && MB_GetCRC(MB->Rx_Buffer, MB->Rx_Buffer[2] + 5) == 0)
// при записи в датчик всегда передаётся 8 байт
#define PR_CheckAnswerCRC (MB->Rx_Buffer[1] == CMD && MB_GetCRC(MB->Rx_Buffer, 8) == 0)
int Parametr_CORR;

/************** ДАТЧИКИ ****************************/
/***************************************************/
// при запуске проверим все сенсоры на активность
void MB_Master_Init(void)
{
	MB_Error_t result = MB_ERROR_NO;
	// после инициализации семафоры установлены, надо их сбросить
/*
	resultSem = osSemaphoreAcquire(TX_Compl_SemHandle, 100/portTICK_RATE_MS);
	resultSem = osSemaphoreAcquire(RX_Compl_SemHandle, 100/portTICK_RATE_MS);
	resultSem = osSemaphoreAcquire(PR_TX_Compl_SemHandle, 100/portTICK_RATE_MS);
	resultSem = osSemaphoreAcquire(PR_RX_Compl_SemHandle, 100/portTICK_RATE_MS);
*/

	// Запросим каждый датчик, если ответит - пометим как активный
//	while (1)	{ // тестовый цикл
	for (int i=0; i<SQ; i++)
	{
		result = Sensor_Read(i);
		if (result == MB_ERROR_NO)
		{
			Sensor_array[i].Active = 1;
			// Инициируем значения в модели для отображения на экране
			Model::setCurrentVal_T(i, Sensor::GetData(TimeFromStart, i, 2));
		}
		else
		{
			// Повторим чтение из датчика ещё три раза
			CountRX = 0;
			for (int var = 0; var < 3; ++var)
			{
				result = Sensor_Read(i);
				if (result == MB_ERROR_NO)
				{
					CountRX++;
				}
			}
			switch (CountRX)
			{
				case 3:		// всё хорошо, датчик отвечает стабильно
					Sensor_array[i].Active = 1;
					// Инициируем значения в модели для отображения на экране
					Model::setCurrentVal_T(i, Sensor::GetData(TimeFromStart, i, 2));
					break;
				default:	// датчик нестабилен
					Sensor_array[i].Active = 0;
					// Изменим цвет поля датчика в модели для отображения на экране

					break;
			}

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
	SW.UART = &huart5;
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
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_COILS, Type4_DO, REG_COUNT, WR_Buffer);
			// Запросим данные со входного регистра модуля ввода-вывода, запишем в Т (Read_Data_2)
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_INPUT, Type4_DI, REG_COUNT, WR_Buffer);
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
		case MB_ERROR_NO:
			Sensor_array[SensIndex].OkCnt++;
			// запись в массив данных
			Sensor::PutData(TimeFromStart, SensIndex, 1, TimeFromStart);
			Sensor::PutData(TimeFromStart, SensIndex, 2, SW.Read_Data_2);	// запись Т
			Sensor::PutData(TimeFromStart, SensIndex, 3, SW.Read_Data_1);	// запись Н
			break;
		case MB_ERROR_DMA_SEND:
			Sensor_array[SensIndex].TxErrorCnt++;
			break;
		case MB_ERROR_UART_SEND:
			Sensor_array[SensIndex].TxErrorCnt++;
			break;
		case MB_ERROR_UART_RECIEVE:	{
			Sensor_array[SensIndex].RxErrorCnt++;
			SW.Read_Data_2 = Sensor::GetData(TimeFromStart-1, SensIndex, 2);	// достали предыдущее значение T
			SW.Read_Data_1 = Sensor::GetData(TimeFromStart-1, SensIndex, 1);	// достали предыдущее значение H
			Sensor::PutData(TimeFromStart, SensIndex, 2, SW.Read_Data_2);		// запись Т
			Sensor::PutData(TimeFromStart, SensIndex, 3, SW.Read_Data_1);		// запись Н
			break;	}
		case MB_ERROR_DMA_RECIEVE:
			Sensor_array[SensIndex].RxErrorCnt++;
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
	else if (huart == &huart4)	// сканирование устройства на шине программирования
	{
//		HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_SET);
//		HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_RESET);
		osSemaphoreRelease(PR_RX_Compl_SemHandle);
//		osDelay(1);	// задержка перед стартовым битом
//		HAL_GPIO_WritePin(MB->PORT, MB->PORT_PIN, GPIO_PIN_SET);
//		HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_RESET);
	}
}

// обработка прерывания - передача завершена
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *Uart) {
	if (Uart == &huart5)
	{
		// Включим направление - приём
		HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET);
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

/*Error handler*/
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


/*Handle Received frame*/
uint16_t MB_TransactionHandler()
{
	uint16_t txLen = 0;
//	/*Check frame length*/
//	if (len < MB_MIN_FRAME_LEN) return txLen;
//	uint8_t Adderss = MB_Slave_Buffer[0];
//	uint8_t Command = MB_Slave_Buffer[1];
//	uint16_t StartReg = MB_Slave_Buffer[3] | MB_Slave_Buffer[2]<<8;
//	uint16_t RegNum = MB_Slave_Buffer[5] | MB_Slave_Buffer[4]<<8;
//	uint16_t CRC_Sum = MB_Slave_Buffer[6] | MB_Slave_Buffer[7]<<8;
//	if (Adderss != (uint8_t) MB_SLAVE_ADDRESS) return txLen;
//	/*Check frame CRC*/
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
/*Handle Read Registers command*/
uint16_t MB_ReadRegsHandler(uint16_t StartReg, uint16_t RegNum) {
//	MB_Error_t error = MB_ERROR_NO;
	uint16_t txLen = 0;
//
//	/*Check address range*/
//	if ((StartReg + RegNum) > MB_SLAVE_REG_COUNT)
//		error = MB_ERROR_WRONG_ADDRESS;
//	/*Check max regs to read*/
//	if (RegNum > 126)
//		error = MB_ERROR_WRONG_VALUE;
//	if (error == MB_ERROR_NO) {
//		uint8_t Bytes = RegNum << 1; /* number of bytes*/
//		MB_Slave_Buffer[2] = Bytes;
//		/*Copy data from memory to frame*/
//		memcpy(&MB_Slave_Buffer[3], &(((uint16_t*) &DFR_Reg)[StartReg]), (int) Bytes);
//		txLen = Bytes + 3;
//		/*Calculate CRC*/
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
// тип 3 - это датчик температуры BlueTooth
			case 3:
			{

				break;
			}
// default  -  к нему относятся:
			// тип 1 - это датчик совмещенного типа Т и Н GL-TH04-MT
			// тип 2 - это датчик температуры РТ100 с RS485
			// тип 4 - это модуль ввода-вывода
			default:
			{
				uint8_t TypeOfSens = Model::Type_of_sensor;
				// цикл будет повторяться, пока оператор не выберет датчик другого типа
				while (Model::Type_of_sensor == TypeOfSens)
				{ // цикл сканирования датчика
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
				} // конец цикла сканирования и записи в датчик
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
/* Функция посылает запрос датчику и принимает ответ.
 * Параметры порта uart, GPIO, буферы и семафоры передачи и приёма передаются в структуре MB.
 * Возвращается статус обработки запроса к датчику.
 */
MB_Error_t Master_Request(MB_Active_t *MB, int N_Bytes)
{
	MB_Error_t MB_ERR = MB_ERROR_NO;
	HAL_StatusTypeDef result;		// status HAL: HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT
//	Вычислим паузу 1000 бит в миллисекундах для ожидания ответа датчика на шине после запроса
	double var = (1000 * 1000) / MB->UART->Init.BaudRate;
	uint8_t pause = uint8_t (var);	// округляем паузу до целой части

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
		resultSem = osSemaphoreAcquire(*MB->Sem_Tx, pause/portTICK_RATE_MS);
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
		resultSem = osSemaphoreAcquire(*MB->Sem_Rx, pause/portTICK_RATE_MS);
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
			Model::T_CORR_sensor = SW.Read_Data_2;
			break; 	}
	// тип датчика: 2 - датчик температуры РТ100 с RS485
		case 2:		{
			// Запросим данные с датчика
			// Одно значение получаем всегда в Read_Data_2
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type2_R, 1, WR_Buffer);
			Model::R_CORR_sensor = SW.Read_Data_2;
			// Здесь паузу между фреймами не делаем, поскольку читаем из того же устройства
			result = Master_RW(&SW, SensAddress, MB_CMD_READ_REGS, Type2_T, 1, WR_Buffer);
			Model::T_CORR_sensor = SW.Read_Data_2;
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

// Функция для передачи данных на сервер
void WriteToServer(uint8_t* Data, int length)
{
	MB_Error_t result;
	MB_Active_t MB;						// объявляем среду работы с шиной

	// Инициируем среду для работы по шине программирования
	MB.UART = &huart4;
	MB.PORT = PROG_MASTER_DE_GPIO_Port;
	MB.PORT_PIN = PROG_MASTER_DE_Pin;
	MB.Sem_Rx = &PR_RX_Compl_SemHandle;
	MB.Sem_Tx = &PR_TX_Compl_SemHandle;
	// копируем данные из структуры Data_TX_Server в буфер передачи
    memcpy(MB.Tx_Buffer, Data, length);
    // пердаем данные в шину
	result = Master_Request(&MB, length);
	if (result != MB_ERROR_NO){

	}
}

