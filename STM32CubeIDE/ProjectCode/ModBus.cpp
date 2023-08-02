#include "main.h"
#include "cmsis_os.h"
#include "ModBus.hpp"
#include "stdio.h"
#include "task.h"
#include "string.h"
#include "queue.h"
//#include "usb_host.h"

#define  MAX_MB_BUFSIZE 50

extern UART_HandleTypeDef huart5;
extern DMA_HandleTypeDef hdma_uart5_rx;
extern DMA_HandleTypeDef hdma_uart5_tx;
//extern UART_HandleTypeDef huart7;
//extern DMA_HandleTypeDef hdma_uart7_rx;
//extern DMA_HandleTypeDef hdma_uart7_tx;

volatile DFR_REGISTERS_t DFR_Reg;
uint8_t MB_Master_Buffer[MAX_MB_BUFSIZE] = {0};
uint16_t master_rec_byte_count = 0;

//volatile uint8_t MB_Slave_Buffer[MAX_MB_BUFSIZE] = {0};
//volatile uint8_t MB_Master_Buffer[MAX_MB_BUFSIZE] = {0};
//uint8_t MB_Slave_Buffer[MAX_MB_BUFSIZE] = {0};
//volatile uint16_t master_rec_byte_count = 0;
//volatile uint16_t slave_rec_byte_count = 0;
//uint16_t slave_rec_byte_count = 0;

uint16_t MB_TransactionHandler();
uint16_t MB_GetCRC(volatile uint8_t* buf, uint16_t len);
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

// start C++ function from C program
extern "C"
{
	void MB_Master_Task_С()	// start from main.c MB_Master_Task
	{
		Start_MB_Master_Task();
	}

}


/************** ДАТЧИКИ ****************************/
/***************************************************/
// Задача MB_Master_Task опрашивает все активные датчики по запросу
void Start_MB_Master_Task(void const *argument)
{
	const uint16_t START_REG = 0, REG_COUNT = 2;
// при запуске сначала проверим все сенсоры на активность
	for (int i=0; i<SQ; i++)
	{
			Sensor_array[i].Active = (MB_Master_Request(Sensor_array[i].Address, 0u, 1u) == MB_ERROR_NO);
	}

//начало основного цикла задачи - опрос активных датчиков
//	while (1) {
//		osDelay(1000u); // для отладки, заменить на таймер
//		for (int i = 0; i < SQ; i++) {
//			if (Sensor_array[i].Active == 0) continue; // посылаем запрос только если датчик числится активным,
//			if (MB_Master_Request(Sensor_array[i].Address, START_REG, REG_COUNT) == MB_ERROR_NO) {
//				// данные приняты - проверяем достоверность
//				if (Sensor_array[i].Address == MB_Master_Buffer[0]
//						&& MB_Master_Buffer[1] == MB_CMD_READ_REGS
//						//(считаем CRC, вместе с принятым CRC, должно быть == 0
//						&& MB_GetCRC(MB_Master_Buffer, MB_Master_Buffer[2] + 5) == 0) {
//					// все проверки ОК, пишем в регистры
//					for (int cnt = 0;  cnt < REG_COUNT; cnt++)
//						((uint16_t*) &DFR_Reg)[START_REG + cnt] = SwapBytes( *(uint16_t*) &MB_Master_Buffer[cnt * 2 +3] );
//					Sensor_array[i].OkCnt++;
//				}
//				else Sensor_array[i].ErrorCnt++;
//			}
//			else	Sensor_array[i].TimeoutCnt++;
//		}
//	}
}


// запрос датчикам на шине ModBus
MB_Error_t MB_Master_Request(uint8_t address, uint16_t StartReg, uint16_t RegNum) {
	MB_Error_t MB_QueueData;	// ответ из очереди
	HAL_StatusTypeDef res;		// status HAL: HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT

	for (int i=3; i--;)
	{ // делаем 3 попытки отправки запроса
		MB_Frame_t *Command = (MB_Frame_t*) MB_Master_Buffer;
		Command->Address = address;
		Command->Command = MB_CMD_READ_REGS;
		Command->StartReg = SwapBytes(StartReg);
		Command->RegNum = SwapBytes(RegNum);
		Command->CRC_Sum = MB_GetCRC(MB_Master_Buffer, 6);
//		HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_SET);
//		if (HAL_UART_Transmit_DMA(&huart7, MB_Master_Buffer, 8) == HAL_OK){
//			while (HAL_UART_GetState(&huart7) == HAL_UART_STATE_BUSY_TX) {
//				osDelay(1);
//			}
//			if (HAL_UARTEx_ReceiveToIdle_DMA(&huart7, MB_Master_Buffer, MAX_MB_BUFSIZE) == HAL_OK)
//				// ответ должен нормально уложиться в 10 ms (19200 -> 500 us на байт
//				//if  (xQueueReceive(MB_MasterQHandle, &MB_QueueData, 10/portTICK_RATE_MS) == pdPASS && MB_QueueData == MB_ERROR_NO)
//				if  (osMessageQueueGet(MB_MasterQHandle, &MB_QueueData, 10/portTICK_RATE_MS) == pdPASS && MB_QueueData == MB_ERROR_NO)
//						return MB_ERROR_NO;
//		}
//		// нет ответа - пробуем еще раз
//		HAL_UART_Abort_IT(&huart7);
//	}
	return MB_ERROR_WRONG_ADDRESS; // не достучались
}


/************************** РАБОТА С СЕРВЕРОМ ********************************/
/*****************************************************************************/



// задача ждет запросы от мастера Modbus и отправляет ответ на запрос
void Start_MB_Slave_Task(void const *argument) {
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
}

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

/**************** ОБРАБОТКА ПРЕРЫВАНИЙ ***************************/
/*****************************************************************/

// обработка прерывания завершён приём
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *Uart) {
//	MB_Error_t res = MB_ERROR_NO; // no err
//	BaseType_t xCoRoutinePreviouslyWoken = pdFALSE;
//
//	if (Uart == &huart5) {
//
//	} else if (Uart == &huart7) {
//		//xQueueSendFromISR(MB_MasterQHandle, &res, &xCoRoutinePreviouslyWoken);
//		osMessageQueuePut(MB_MasterQHandle, &res, &xCoRoutinePreviouslyWoken);
//	}
}

// обработка прерывания ошибки
void HAL_UART_ErrorCallback(UART_HandleTypeDef *Uart) {
//	MB_Error_t res = MB_ERROR_COMMAND;
//	BaseType_t xCoRoutinePreviouslyWoken = pdFALSE;
//	if (Uart == &huart5) {
//		HAL_GPIO_WritePin(MB_SLAVE_DE_GPIO_Port, MB_SLAVE_DE_Pin, GPIO_PIN_RESET); // сброс бита DE RS-485
//		xQueueSendFromISR(MB_SlaveQHandle, &res, &xCoRoutinePreviouslyWoken);
//	} else if (Uart == &huart7) {
//		uint16_t stat = Uart->Instance->SR;
//		Uart->Instance->DR = 0;
//		xQueueSendFromISR(MB_MasterQHandle, &res, &xCoRoutinePreviouslyWoken);
//	}
}

// обработка прерывания приём завершён RX Event callback
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
//	MB_Error_t res = MB_ERROR_NO; // no err
//	BaseType_t xCoRoutinePreviouslyWoken = pdFALSE;
//
// 	if (huart == &huart5) {
//		slave_rec_byte_count = MAX_MB_BUFSIZE - hdma_uart5_rx.Instance->NDTR;
//		xQueueSendFromISR(MB_SlaveQHandle, &res, &xCoRoutinePreviouslyWoken);
//	} else if (huart == &huart7) {
//		// Reset TC flag
////		uint16_t stat = huart->Instance->SR; if (stat);
////		huart->Instance->DR = 0;
//		xQueueSendFromISR(MB_MasterQHandle, &res, &xCoRoutinePreviouslyWoken);
//	}
}

// обработка прерывания передача завершена
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *Uart) {
//	if (Uart == &huart5) {
//		HAL_GPIO_WritePin(MB_SLAVE_DE_GPIO_Port, MB_SLAVE_DE_Pin, GPIO_PIN_RESET); // сброс бита DE RS-485
//	} else if (Uart == &huart7) {
//		HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET); // сброс бита DE RS-485}
//	}
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

// отладочная задача - рандомно изменяет регистры для проверки отображения
//void Start_LedTask(void const * argument)
//{
//  for(;;)
//  {
//		HAL_GPIO_TogglePin(RedLED_GPIO_Port, RedLED_Pin);
//		uint32_t trngN = HAL_RNG_GetRandomNumber(&hrng);
//		if (trngN == trng) { // ошибка RND
//		}
//		trng = trngN;
//	int regnum = trng & 0xFU;
//		uint16_t* Registers = (uint16_t*) &DFR_Reg;
//		if (regnum<8 && regnum>1) {
//			int16_t current = Registers[regnum];
//			if (current < 900) Registers[regnum]++;
//			else Registers[regnum]=regnum & 1 ? -150 : 200;
//		}
//		// биты
//		int bit =  trng>>5 & 0xFU;
//		DFR_Reg.DFR_flags ^= (1<<bit);
//		osDelay(200);
//
//  }
//}


void Start_LedTask(void const * argument);


/* Инициализация задач, очередей и тд, вызывается из main() */
void MB_Init(void) {
/* Очередь для MB Slave */
//osMessageQDef(MB_SlaveQ, 1, uint16_t);
//MB_SlaveQHandle = osMessageCreate(osMessageQ(MB_SlaveQ), NULL);
//
///* Очередь для MB Master */
//osMessageQDef(MB_MasterQ, 1, uint16_t);
//MB_MasterQHandle = osMessageCreate(osMessageQ(MB_MasterQ), NULL);


/* definition and creation of LedTask */
/*
osThreadDef(LedTask, Start_LedTask, osPriorityIdle, 0, 200);
LedTaskHandle = osThreadCreate(osThread(LedTask), NULL);
*/

/* definition and creation of MB_Slave_Task
osThreadDef(MB_Slave_Task, Start_MB_Slave_Task, osPriorityNormal, 0, 1000);
MB_Slave_TaskHandle = osThreadCreate(osThread(MB_Slave_Task), NULL);
*/

/* definition and creation of MB_Master_Task
osThreadDef(MB_Master_Task, Start_MB_Master_Task, osPriorityNormal, 0, 1000);
MB_Master_TaskHandle = osThreadCreate(osThread(MB_Master_Task), NULL);
*/

//HAL_GPIO_WritePin(MB_MASTER_DE_GPIO_Port, MB_MASTER_DE_Pin, GPIO_PIN_RESET);
//HAL_GPIO_WritePin(MB_SLAVE_DE_GPIO_Port, MB_SLAVE_DE_Pin, GPIO_PIN_RESET);
//HAL_GPIO_WritePin(GreenLED_GPIO_Port, GreenLED_Pin, GPIO_PIN_RESET);

//trng = HAL_RNG_GetRandomNumber(&hrng);


}

