/*
 * ModBus.hpp
 *
 *  Created on: Aug 1, 2023
 *      Author: gdr
 */

/*
 * defrost.h
 *
 *  Created on: Jun 27, 2023
 *      Author: nickn
 */

#ifndef MODBUS_HPP_
#define MODBUS_HPP_

#include "main.h"
#include "cmsis_os.h"
#include "Data.hpp"
#include "stm32f4xx_hal.h"

#define MB_SLAVE_ADDRESS	200
#define BAUD_RATE_NUMBER	8

/* Описание набора датчиков дефростера */
typedef struct {
	uint8_t	TypeNumber;
	char TypeName[11];
} SENSOR_Type_t;

typedef struct {
uint8_t Address;			// ModBus address
uint8_t BaudRate;			// ModBus baud rate
uint8_t Active;				// флаг активности датчика
uint8_t TypeOfSensor;		// тип датчика: 1 - совмещённый датчик температуры и влажности
char PositionName[11];	// наименование позиции датчика
// статистика полученных ответов от датчика
uint16_t OkCnt;				// счётчик ответов
uint16_t ErrCnt;			// счётчик неответов
uint16_t TxErrorCnt;		// счётчик ошибок передачи данных, CRC
uint16_t RxErrorCnt;		// счётчик ошибок приёма данных, CRC
} SENSOR_typedef_t;

#define SwapBytes(data) ( (((data) >> 8) & 0x00FF) | (((data) << 8) & 0xFF00) )
//(считаем CRC всей посылки, вместе с принятым CRC, должно быть = 0


/* РЕГИСТРЫ ОПИСЫВАЮЩИЕ СОСТОЯНИЕ ДЕФРОСТЕРА */
#define MB_SLAVE_REG_COUNT	11
typedef struct
{
//	int16_t		DFR_Hum_In;			// Влажность входная				1
//	int16_t		DFR_Temp_In;		// Температура входная				2
//	int16_t		DFR_Hum_Out_Left;	// Влажность выходная левая			3
//	int16_t		DFR_Temp_Out_Left;	// Температура выходная левая		4
//	int16_t		DFR_Hum_Out_Right;	// Влажность выходная правая		5
//	int16_t		DFR_Temp_Out_Right;	// Температура выходная правая		6
//	int16_t		DFR_Hum_Prod_Left;	// Влажность продукта левая			7
//	int16_t		DFR_Temp_Prod_Left;	// Температура продукта левая		8
//	int16_t		DFR_Hum_Prod_Right;	// Влажность продукта правая		9
//	int16_t		DFR_Temp_Prod_Right;// Температура продукта правая		10
// ФлагиСостояния, битовый регистр, Имена битов
//	uint16_t DFR_flags;			    // доступ к регистру флагов целиком	11

	uint16_t DFR_Ten1_Left:1;		// Тэн1 левый
	uint16_t DFR_Ten2_Left:1;		// Тэн2 левый
	uint16_t DFR_Ten3_Left:1;		// Тэн3 левый
	uint16_t DFR_Ten1_Right:1;		// Тэн1 правый
	uint16_t DFR_Ten2_Right:1;		// Тэн2 правый
	uint16_t DFR_Ten3_Right:1;		// Тэн3 правый
	uint16_t DFR_Vent1_Left:1;		// Вентилятор1 левый
	uint16_t DFR_Vent2_Left:1;		// Вентилятор2 левый
	uint16_t DFR_Vent1_Right:1;	    // Вентилятор1 правый
	uint16_t DFR_Vent2_Right:1; 	// Вентилятор2 правый
	uint16_t DFR_Water_Flap:1;		// Водный клапан

} DFR_REGISTERS_t;

// Modbus request frame
typedef struct {
	uint8_t Address;
	uint8_t Command;
	uint16_t StartReg;
	uint16_t RegNum;
	uint16_t CRC_Sum;
} MB_Frame_t;

typedef enum
{
    MB_CMD_READ_REGS = 0x03,
    MB_CMD_WRITE_REG = 0x06,
    MB_CMD_WRITE_REGS = 0x10
}MB_Command_t;

// Ошибки работы с шиной ModBus
typedef enum
{
	MB_ERROR_NO = 0x00,
	MB_ERROR_COMMAND = 0x01,
	MB_ERROR_WRONG_ADDRESS = 0x02,
	MB_ERROR_WRONG_VALUE = 0x03,
	MB_ERROR_DMA_SEND = 0x04,
	MB_ERROR_UART_SEND = 0x05,
	MB_ERROR_UART_RECIEVE = 0x06,
	MB_ERROR_DMA_RECIEVE = 0x07
}MB_Error_t;

// Адреса регистров устройств на шине
typedef enum
{
	Type1_H = 0x0000,
	Type1_T = 0x0001,
	Type1_T_calibr = 0x0050,
	Type1_H_calibr = 0x0051,
	Type1_Addr = 0x07D0,
	Type1_Baud = 0x07D1,

	Type2_T = 0x0000,
	Type2_R = 0x0020,
	Type2_T_calibr = 0x0040,
	Type2_R_calibr = 0x0060,
	Type2_T_unit = 0x00F9,
	Type2_Automatic = 0x00FA,
	Type2_reset = 0x00FB,
	Type2_delay = 0x00FC,
	Type2_Addr = 0x00FD,
	Type2_Baud = 0x00FE,
	Type2_Parity = 0x00FF
}MB_Reg_t;

//osThreadId MB_Slave_TaskHandle;
extern osThreadId MB_Master_TaskHandle;
extern osMessageQId MB_MasterQHandle;
MB_Error_t Sensor_Read(uint8_t SensIndex);

void MB_Master_Init(void);
void ProgrammingSensor(void);
void PR_UART4_Init(int BaudRateValue);
MB_Error_t Sensor_Read_CORR_param(uint8_t SensIndex);


#endif /* MODBUS_HPP_ */


