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

#define  MAX_MB_BUFSIZE 20

extern UART_HandleTypeDef huart4;				// для программирования датчиков
extern UART_HandleTypeDef huart5;				// для считывания данных с датчиков
extern osSemaphoreId_t TX_Compl_SemHandle;		// семафор окончания передачи датчикам
extern osSemaphoreId_t RX_Compl_SemHandle;		// семафор окончания приёма от датчиков
extern osSemaphoreId_t PR_TX_Compl_SemHandle;	// семафор окончания приёма при программировании
extern osSemaphoreId_t PR_RX_Compl_SemHandle;	// семафор окончания передачи при программировании
// current number of measure
extern unsigned int TimeFromStart;


//osMessageQId MB_SlaveQHandle;
//extern UART_HandleTypeDef huart7;
//extern DMA_HandleTypeDef hdma_uart7_rx;
//extern DMA_HandleTypeDef hdma_uart7_tx;

volatile DFR_REGISTERS_t DFR_Reg;
uint8_t MB_MasterTx_Buffer[MAX_MB_BUFSIZE] = {0};
uint8_t MB_MasterRx_Buffer[MAX_MB_BUFSIZE] = {0};
uint8_t PR_MasterTx_Buffer[MAX_MB_BUFSIZE] = {0};
uint8_t PR_MasterRx_Buffer[MAX_MB_BUFSIZE] = {0};
uint16_t master_rec_byte_count = 0;
uint16_t CountRX = 0;

uint16_t MB_TransactionHandler();
uint16_t MB_GetCRC(volatile uint8_t* buf, uint16_t len);
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

SENSOR_typedef_t Sensor_array[SQ] =
{
		{101,3,0,1,"Ldf", 0,0,0,0},		// 0 - defroster left
		{102,3,0,1,"Rdf",0,0,0,0},		// 1 - defroster right
		{103,3,0,1,"IN",0,0,0,0},		// 2 - defroster center
		{104,3,0,0,"Lpr",0,0,0,0},		// 3 - fish left
		{105,3,0,0,"Rpr",0,0,0,0},		// 4 - fish right
};

uint8_t SensPortNumber;					// номер порта на шине для устройства (чтение) - адрес регистра в датчике (запись)
uint8_t SensBaudRateIndex;				// индекс в массиве скорости шины
uint8_t SensNullValue = 255;
int Sens_WR_value;						// переменная для чтения записанного в датчик значения
// массив скорости шины
int BaudRate[8] = {2400, 4800, 9600, 19200, 38400, 57600, 115200, 1200};
typedef struct
{
	uint8_t *Tx_Buffer;		// указатель на буфер передачи
	uint8_t *Rx_Buffer;		// указатель на буфер приёма
	UART_HandleTypeDef &UART = huart4;
	GPIO_TypeDef *GPIO;
	uint16_t PIN;
	osSemaphoreId_t &Sem_Tx = PR_TX_Compl_SemHandle;
	osSemaphoreId_t &Sem_Rx = PR_RX_Compl_SemHandle;

} MB_Active_t;

MB_Error_t PR_Master_Request(MB_Active_t);


/************** ДАТЧИКИ ****************************/
/***************************************************/
// при запуске сначала проверим все сенсоры на активность
void MB_Master_Init(void) {
	MB_Error_t result;
	// параметры для датчика совмещенного типа
	// после инициализации семафоры установлены, надо их сбросить
	resultSem = osSemaphoreAcquire(TX_Compl_SemHandle, 100/portTICK_RATE_MS);
	resultSem = osSemaphoreAcquire(RX_Compl_SemHandle, 100/portTICK_RATE_MS);
	//*****************************************


	//*****************************************

		// Запросим каждый датчик, если ответит - пометим как активный
		for (int i=0; i<SQ; i++)
		{
			result = MB_Master_Read(i);
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
					result = MB_Master_Read(i);
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
}

// Функция считывает данные с датчика
//MB_Error_t MB_Master_Read(int i)
//{
//	// параметры для датчика совмещенного типа
//	const uint16_t START_REG = 0, REG_COUNT = 2;
//	int T, H;
//	MB_Error_t result;
//
//
//	result = MB_Master_Request(Sensor_array[i].Address, START_REG, REG_COUNT);
//	T = 0;
//	H = 0;
//			switch (result) {
//				case MB_ERROR_NO:
//					// данные приняты - проверяем достоверность
//					//(считаем CRC, вместе с принятым CRC, должно быть == 0
//					if (Sensor_array[i].Address == MB_MasterRx_Buffer[0]
//						&& MB_MasterRx_Buffer[1] == MB_CMD_READ_REGS
//						&& MB_GetCRC(MB_MasterRx_Buffer, MB_MasterRx_Buffer[2] + 5) == 0)
//					{
//						// все проверки ОК, пишем значения с датчика совмещённого типа
//						H = SwapBytes( *(uint16_t*) &MB_MasterRx_Buffer[3]);
//						T = SwapBytes( *(uint16_t*) &MB_MasterRx_Buffer[5]);
//						Sensor_array[i].OkCnt++;
//					}
//					else
//					{
//						Sensor_array[i].ErrCnt++;
//						result = MB_ERROR_UART_SEND;
//					}
//					break;
//				case MB_ERROR_DMA_SEND:
//					Sensor_array[i].TxErrorCnt++;
//					break;
//				case MB_ERROR_UART_SEND:
//					Sensor_array[i].TxErrorCnt++;
//					break;
//				case MB_ERROR_UART_RECIEVE:
//					Sensor_array[i].RxErrorCnt++;
//					break;
//				case MB_ERROR_DMA_RECIEVE:
//					Sensor_array[i].RxErrorCnt++;
//					break;
//				default:
//					break;
//			}
//			// запись в массив данных
//			Sensor::PutData(TimeFromStart, i, 1, TimeFromStart);
//			Sensor::PutData(TimeFromStart, i, 2, T);
//			Sensor::PutData(TimeFromStart, i, 3, H);
//			return result;
//}
MB_Error_t MB_Master_Read(int i)
{
	// параметры для датчика совмещенного типа
	const uint16_t START_REG = 0, REG_COUNT = 2;
	int T, H;
	MB_Error_t result;
	MB_Active_t MB;

	// Выполним приведение типа: указателю Command присвоим указатель буфера, буфер примет тип MB_Frame_t
	MB_Frame_t *Command = (MB_Frame_t*) MB_MasterTx_Buffer;
	memset(MB_MasterTx_Buffer, 0, MAX_MB_BUFSIZE);
	memset(MB_MasterRx_Buffer, 0, MAX_MB_BUFSIZE);
	// Заполним начало буфера структурой для отправки команды датчику
	Command->Address = Sensor_array[i].Address;
	Command->Command = MB_CMD_READ_REGS;
	Command->StartReg = SwapBytes(START_REG);
	Command->RegNum = SwapBytes(REG_COUNT);
	Command->CRC_Sum = MB_GetCRC(MB_MasterTx_Buffer, 6);
	// Инициируем среду для программирования датчика
	MB.Tx_Buffer = MB_MasterTx_Buffer;
	MB.Rx_Buffer = MB_MasterRx_Buffer;
	MB.UART = huart5;
	MB.GPIO = MB_MASTER_DE_GPIO_Port;
	MB.PIN = MB_MASTER_DE_Pin;
	MB.Sem_Rx = RX_Compl_SemHandle;
	MB.Sem_Tx = TX_Compl_SemHandle;


//	result = MB_Master_Request(Sensor_array[i].Address, START_REG, REG_COUNT);
	result = PR_Master_Request(MB);
	T = 0;
	H = 0;
			switch (result) {
				case MB_ERROR_NO:
					// данные приняты - проверяем достоверность
					//(считаем CRC, вместе с принятым CRC, должно быть == 0
					// данные приняты - проверяем достоверность
					if CheckAnswerCRC_MB
					{	// все проверки ОК, пишем значения с датчика совмещённого типа
						H = SwapBytes( *(uint16_t*) &MB_MasterRx_Buffer[3]);
						T = SwapBytes( *(uint16_t*) &MB_MasterRx_Buffer[5]);
						Sensor_array[i].OkCnt++;
					}
					else
					{
						Sensor_array[i].ErrCnt++;
						result = MB_ERROR_UART_SEND;
					}
					break;
				case MB_ERROR_DMA_SEND:
					Sensor_array[i].TxErrorCnt++;
					break;
				case MB_ERROR_UART_SEND:
					Sensor_array[i].TxErrorCnt++;
					break;
				case MB_ERROR_UART_RECIEVE:
					Sensor_array[i].RxErrorCnt++;
					break;
				case MB_ERROR_DMA_RECIEVE:
					Sensor_array[i].RxErrorCnt++;
					break;
				default:
					break;
			}
			// запись в массив данных
			Sensor::PutData(TimeFromStart, i, 1, TimeFromStart);
			Sensor::PutData(TimeFromStart, i, 2, T);
			Sensor::PutData(TimeFromStart, i, 3, H);
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
MB_Error_t MB_Master_Request(uint8_t address, uint16_t StartReg, uint16_t RegNum)
{
	MB_Error_t MB_ERR = MB_ERROR_NO;
	HAL_StatusTypeDef result;		// status HAL: HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT
	// Выполним приведение типа: указателю Command присвоим указатель буфера, буфер примет тип MB_Frame_t
	MB_Frame_t *Command = (MB_Frame_t*) MB_MasterTx_Buffer;

	memset(MB_MasterTx_Buffer, 0, MAX_MB_BUFSIZE);
	memset(MB_MasterRx_Buffer, 0, MAX_MB_BUFSIZE);
	// Заполним начало буфера структурой для отправки команды датчику
	Command->Address = address;
	Command->Command = MB_CMD_READ_REGS;
	Command->StartReg = SwapBytes(StartReg);
	Command->RegNum = SwapBytes(RegNum);
	Command->CRC_Sum = MB_GetCRC(MB_MasterTx_Buffer, 6);

	// ПЕРЕДАЧА DMA ********************************
	// Включим направление - передача
	HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_SET);
	// Начинаем передачу отправкой буфера с записанной структурой в порт UART через DMA
	result = HAL_UART_Transmit_DMA(&huart5, MB_MasterTx_Buffer, 8);
	if (result == HAL_OK)
	{
		// ПЕРЕДАЧА UART ***************************
		// Ждём, пока UART всё передаст в шину и обработчик прерывания HAL_UART_TxCpltCallback выдаст токен семафора
		resultSem = osSemaphoreAcquire(TX_Compl_SemHandle, 150/portTICK_RATE_MS);
		if (resultSem != osOK)
		{	// обработка ошибки передачи по UART
			MB_ERR = MB_ERROR_UART_SEND;
			HAL_UART_AbortTransmit_IT(&huart5);
			// Включим направление - приём
			HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET);
			return MB_ERR;
		}
		// Направление на приём включается в обработчике прерывания HAL_UART_TxCpltCallback

		// ПРИЁМ DMA *******************************
		// Инициируем приём с использованием DMA
		osDelay(1);	// BUSY RX
		result = HAL_UARTEx_ReceiveToIdle_DMA(&huart5, MB_MasterRx_Buffer, MAX_MB_BUFSIZE);
		if (result == HAL_OK)
		{	// ReceiveToIdle_DMA отработал и вышел по тайм-ауту
			// последнее значение в очереди = 0, ждём прерывание приёма по IDLE
			// Ждём, когда приём закончится и прерывание выдаст токен семафора
			//ответ должен нормально уложиться в 10 ms (19200 -> 500 us на байт), это время функция ждёт токен семафора в состоянии блокировки
			resultSem = osSemaphoreAcquire(RX_Compl_SemHandle, 150/portTICK_RATE_MS);
			osDelay(1);
			if (resultSem != osOK)
			{	// прерывания не случилось, семафора не дождались, вышли по тайм-ауту
				MB_ERR = MB_ERROR_UART_RECIEVE;
				// датчик не ответил, прекращаем ReceiveToIdle_DMA
				HAL_UART_AbortReceive_IT(&huart5);
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
		HAL_UART_AbortTransmit_IT(&huart5);
		// Включим направление - приём
		HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET);
	}
	return MB_ERR;
}

/**************** ОБРАБОТКА ПРЕРЫВАНИЙ ***************************/
/*****************************************************************/

// обработка прерывания "завершён полный приём", сюда может попасть только если выполнен полный заданный приём
// однако обычно размер пакета неопределён и указывается для приёма большой буфер, который никогда не будет заполнен
// нормальное завершение приёма - это событие, например IDLE, остановка принимаемой информации
// это событие обрабатывается прерыванием HAL_UARTEx_RxEventCallback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *Uart) {
}

// обработка прерывания ошибки
void HAL_UART_ErrorCallback(UART_HandleTypeDef *Uart) {
//	MB_Error_t res = MB_ERROR_COMMAND;
	HAL_UART_AbortTransmit_IT(Uart);
//	BaseType_t xCoRoutinePreviouslyWoken = pdFALSE;
	/*if (Uart == &huart7)		// ошибка от компьютера
	{
		HAL_GPIO_WritePin(MB_SLAVE_DE_GPIO_Port, MB_SLAVE_DE_Pin, GPIO_PIN_RESET); // сброс бита DE RS-485
		xQueueSendFromISR(MB_SlaveQHandle, &res, &xCoRoutinePreviouslyWoken);
	}
	else*/
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
//	MB_Error_t res = MB_ERROR_NO; // no err
//	BaseType_t xCoRoutinePreviouslyWoken = pdFALSE;

/* 	if (huart == &huart7)		//	приём от компьютера
 	{
//		slave_rec_byte_count = MAX_MB_BUFSIZE - hdma_uart5_rx.Instance->NDTR;
//		xQueueSendFromISR(MB_SlaveQHandle, &res, &xCoRoutinePreviouslyWoken);
	}
 	else*/
	if (huart == &huart5)		// приём от датчика
 	{
		// Установим семафор окончания приёма, продолжится задача ReadData
		osSemaphoreRelease(RX_Compl_SemHandle);
	}
	else if (huart == &huart4)	// сканирование устройства на шине программирования
	{
		osSemaphoreRelease(PR_RX_Compl_SemHandle);
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
//	} else if (Uart == &huart7) {
//		HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET); // сброс бита DE RS-485}
	}
	else if (Uart == &huart4)
	{
		HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_RESET);
		osSemaphoreRelease(PR_TX_Compl_SemHandle);
	}
}

//************************** РАБОТА С СЕРВЕРОМ ********************************/
/*****************************************************************************/



// задача ждет запросы от мастера Modbus и отправляет ответ на запрос
//void Start_MB_Slave_Task(void const *argument) {
//	MB_Error_t MB_QueueData; // ответ из очереди
//
//	MB_Slave_TaskHandle = xTaskGetCurrentTaskHandle();
//
//	do { // ждем запросов
//		if (HAL_UARTEx_ReceiveToIdle_DMA(&huart5, MB_Slave_Buffer, sizeof(MB_Slave_Buffer)) != HAL_OK) {
//			HAL_UART_Abort_IT(&huart7);
//			continue;
//		}
//		if (xQueueReceive(MB_SlaveQHandle, &MB_QueueData, portMAX_DELAY)) {
//			if (MB_QueueData == MB_ERROR_NO) {
//				uint16_t TxLen = MB_TransactionHandler();
//				// если длина ненулевая - отсылаем пакет
//				if (TxLen > 0) {
//					HAL_GPIO_WritePin(MB_SLAVE_DE_GPIO_Port, MB_SLAVE_DE_Pin, GPIO_PIN_SET);
//					if (HAL_UART_Transmit_DMA(&huart5, MB_Slave_Buffer, TxLen)== HAL_OK) {
//						while (HAL_UART_GetState(&huart5) != HAL_UART_STATE_READY)
//							osDelay(1);
//					}
//				}
//			}
//		}
//	} while (1);
//}

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
	uint8_t OldBaudRate = 0;
	uint8_t OldAddress = 0;
	uint8_t WR_BaudRate = 0;
	uint8_t WR_Address = 0;
	MB_Error_t result = MB_ERROR_NO;
	// после инициализации семафоры установлены, надо их сбросить
	resultSem = osSemaphoreAcquire(PR_TX_Compl_SemHandle, 100/portTICK_RATE_MS);
	resultSem = osSemaphoreAcquire(PR_RX_Compl_SemHandle, 100/portTICK_RATE_MS);
	// датчики не искали, выведем на экран инфо об их отсутствии
	Model::setCurrentVal_PR(SensNullValue, SensNullValue);

	while (1)
	{
		result = ScanSensor();
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
		osDelay(10);

		// запись данных в датчик, если флаг установлен
		uint8_t i = 0;
		while (Model::Flag_WR_to_sensor == 1)
		{
			// скорость шины установлена той, на которой датчик работает
			// адрес датчика принят и записан в SensPortNumber
			// устанавливаем данные для записи нового адреса порта
			WR_BaudRate = Model::BaudRate_WR_to_sensor;
			WR_Address = Model::Address_WR_to_sensor;
			// запись и чтение адреса
			result = PR_Master_RW(SensPortNumber, MB_CMD_WRITE_REG, 0x7D0, WR_Address);
			// проверка записанного
			if (result == MB_ERROR_NO)
			{	//всё хорошо, датчик записан
				if (Sens_WR_value == WR_Address)
				{	// записываем и читаем скорость
					SensPortNumber = Sens_WR_value;
					osDelay(10);	// время на переключение датчика на новые параметры
					result = PR_Master_RW(SensPortNumber, MB_CMD_WRITE_REG, 0x7D1, WR_BaudRate);
					if (result == MB_ERROR_NO)
					{	//всё хорошо, датчик записан
						// сбрасываем флаг записи в датчик
						Model::Flag_WR_to_sensor = 0;
						SensBaudRateIndex = Sens_WR_value;
						osDelay(10);	// время на переключение датчика на новые параметры
					}
					else
					{	// плохо, что-то не получилось
						// повторяем запись адреса ещё 3 раза
						if (i++ == 3)
							Model::Flag_WR_to_sensor = 0;
						// надо как-то оповестить об ошибке
					}
				}
				else
				{	// плохо, что-то не получилось
					Model::Flag_WR_to_sensor = 0;
					// надо как-то оповестить об ошибке
				}
			} // конец проверки записанного
			else
				Model::Flag_WR_to_sensor = 0;
			// надо как-то оповестить об ошибке

		} // конец записи в датчик
	} // конец цикла

}

MB_Error_t ScanSensor()
{
	MB_Error_t result = MB_ERROR_NO;
	// Производим сканирование широковещательной посылкой шины на всех скоростях
	for (int i = 0; i < BAUD_RATE_NUMBER; ++i)
	{
		PR_UART4_Init(BaudRate[i]);
		result = PR_Master_RW(0xFF, MB_CMD_READ_REGS, 0x7D0, 2);
		osDelay(10);

		if (result == MB_ERROR_NO) break;
	}
	return result;
}

// Функция считывает данные с датчика
MB_Error_t PR_Master_RW(int Address, MB_Command_t CMD, uint16_t START_REG, uint16_t DATA)
{
	// параметры для датчика совмещенного типа
	MB_Error_t result;
	MB_Active_t MB;

	// Выполним приведение типа: указателю Command присвоим указатель буфера, буфер примет тип MB_Frame_t
	MB_Frame_t *Command = (MB_Frame_t*) PR_MasterTx_Buffer;
	memset(PR_MasterTx_Buffer, 0, MAX_MB_BUFSIZE);
	memset(PR_MasterRx_Buffer, 0, MAX_MB_BUFSIZE);
	// Заполним начало буфера структурой для отправки команды датчику
	Command->Address = Address;
	Command->Command = CMD;
	Command->StartReg = SwapBytes(START_REG);
	Command->RegNum = SwapBytes(DATA);
	Command->CRC_Sum = MB_GetCRC(PR_MasterTx_Buffer, 6);
	// Инициируем среду для программирования датчика
	MB.Tx_Buffer = PR_MasterTx_Buffer;
	MB.Rx_Buffer = PR_MasterRx_Buffer;
	MB.UART = huart4;
	MB.GPIO = PROG_MASTER_DE_GPIO_Port;
	MB.PIN = PROG_MASTER_DE_Pin;
	MB.Sem_Rx = PR_RX_Compl_SemHandle;
	MB.Sem_Tx = PR_TX_Compl_SemHandle;

	result = PR_Master_Request(MB);
	switch (result) {
		case MB_ERROR_NO:
			// данные приняты - проверяем достоверность
			if CheckAnswerCRC_PR
			{	// все проверки ОК, пишем значения с датчика совмещённого типа
				if (CMD != MB_CMD_WRITE_REG)
				{ 	// заказывали два регистра на чтение
					SensPortNumber = SwapBytes( *(uint16_t*) &PR_MasterRx_Buffer[3]);
					SensBaudRateIndex = SwapBytes( *(uint16_t*) &PR_MasterRx_Buffer[5]);
				}
				else
				{	// всегда один регистр для записи
					Sens_WR_value = SwapBytes( *(uint16_t*) &PR_MasterRx_Buffer[4]);
				}
			}
			else
			{	// проверки не пройдены, ошибка в принятых данных
				result = MB_ERROR_UART_SEND;
			};

			break;
		case MB_ERROR_DMA_SEND:
			break;
		case MB_ERROR_UART_SEND:
			break;
		case MB_ERROR_UART_RECIEVE:
			break;
		case MB_ERROR_DMA_RECIEVE:
			break;
		default:
			break;
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
MB_Error_t PR_Master_Request(MB_Active_t MB)
{
	MB_Error_t MB_ERR = MB_ERROR_NO;
	HAL_StatusTypeDef result;		// status HAL: HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT

	// ПЕРЕДАЧА DMA ********************************
	// Включим направление - передача
	HAL_GPIO_WritePin(MB.GPIO, MB.PIN, GPIO_PIN_SET);
	// Начинаем передачу отправкой буфера с записанной структурой в порт UART через DMA
	result = HAL_UART_Transmit_DMA(&MB.UART, MB.Tx_Buffer, 8);
	if (result == HAL_OK)
	{
		// ПЕРЕДАЧА UART ***************************
		// Ждём, пока UART всё передаст в шину и обработчик прерывания HAL_UART_TxCpltCallback выдаст токен семафора
		resultSem = osSemaphoreAcquire(MB.Sem_Tx, 150/portTICK_RATE_MS);
		if (resultSem != osOK)
		{	// обработка ошибки передачи по UART
			MB_ERR = MB_ERROR_UART_SEND;
			HAL_UART_AbortTransmit_IT(&MB.UART);
			// Включим направление - приём
			HAL_GPIO_WritePin(MB.GPIO, MB.PIN, GPIO_PIN_RESET);
			return MB_ERR;
		}
		// Направление на приём включается в обработчике прерывания HAL_UART_TxCpltCallback

		// ПРИЁМ DMA *******************************
		// Инициируем приём с использованием DMA
		osDelay(1);	// BUSY RX
		result = HAL_UARTEx_ReceiveToIdle_DMA(&MB.UART, MB.Rx_Buffer, MAX_MB_BUFSIZE);
		if (result == HAL_OK)
		{	// ReceiveToIdle_DMA отработал и вышел по тайм-ауту
			// последнее значение в очереди = 0, ждём прерывание приёма по IDLE
			// Ждём, когда приём закончится и прерывание выдаст токен семафора
			//ответ должен нормально уложиться в 11 байт (1200 -> 6,7 ms на байт, всего 73,3 ms), это время функция ждёт токен семафора в состоянии блокировки
			resultSem = osSemaphoreAcquire(MB.Sem_Rx, 200/portTICK_RATE_MS);
			osDelay(1);
			if (resultSem != osOK)
			{	// прерывания не случилось, семафора не дождались, вышли по тайм-ауту
				MB_ERR = MB_ERROR_UART_RECIEVE;
				// датчик не ответил, прекращаем ReceiveToIdle_DMA
				HAL_UART_AbortReceive_IT(&MB.UART);
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
		HAL_UART_AbortTransmit_IT(&MB.UART);
		// Включим направление - приём
		HAL_GPIO_WritePin(MB.GPIO, MB.PIN, GPIO_PIN_RESET);
	}
	return MB_ERR;
}

void PR_UART4_Init(int BaudRateValue)
{
  huart4.Instance = UART4;
  huart4.Init.BaudRate = BaudRateValue;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
}
