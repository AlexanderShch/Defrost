/*
 * CommandReceiver.hpp
 *
 *  Created on: October 23, 2025
 *      Author: System
 *  Description: Модуль приема и обработки команд от сервера через COM-порт
 */

#ifndef COMMANDRECEIVER_HPP_
#define COMMANDRECEIVER_HPP_

#include "main.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

// Максимальная длина команды (заголовок + данные + CRC)
#define CMD_MAX_LENGTH 64
#define CMD_MAX_DATA_LENGTH 59
#define CMD_HEADER_SIZE 3
#define CMD_CRC_SIZE 2

// Типы команд (CommandType)
typedef enum {
    CMD_TYPE_PROG_CONTROL   = 0x01,  // Команды управления программой (СТАРТ, СТОП и т.д.)
    CMD_TYPE_CONFIGURATION  = 0x02,  // Команды конфигурации
    CMD_TYPE_REQUEST        = 0x03,  // Команды запроса данных
    CMD_TYPE_DEVICE_CONTROL = 0x04   // Команды управления устройствами
} CommandType_t;

// Тип для ответов (используется внутри модуля)
#define CMD_TYPE_RESPONSE       0x80   // Ответы от устройства

// Коды команд управления программой (ProgControlCommand)
typedef enum {
    PROG_CTRL_CMD_START   = 0x01,  // Запуск программы
    PROG_CTRL_CMD_STOP    = 0x02,  // Остановка программы
    PROG_CTRL_CMD_PAUSE   = 0x03,  // Приостановка программы
    PROG_CTRL_CMD_RESUME  = 0x04,  // Возобновление программы
    PROG_CTRL_CMD_RESET   = 0x05   // Сброс программы
} ProgControlCommand_t;

// Коды команд управления устройствами (DeviceControlCommand)
typedef enum {
    DEV_CTRL_CMD_RELAY_ON    = 0x01,  // Включить реле
    DEV_CTRL_CMD_RELAY_OFF   = 0x02,  // Выключить реле
    DEV_CTRL_CMD_RELAY_SET   = 0x03,  // Установить состояние реле (битовая маска)
    DEV_CTRL_CMD_HEATER_ON   = 0x04,  // Включить нагреватели
    DEV_CTRL_CMD_HEATER_OFF  = 0x05,  // Выключить нагреватели
    DEV_CTRL_CMD_FAN_ON      = 0x06,  // Включить вентиляторы
    DEV_CTRL_CMD_FAN_OFF     = 0x07   // Выключить вентиляторы
} DeviceControlCommand_t;

// Коды команд конфигурации (ConfigCommand)
typedef enum {
    CFG_CMD_SET_TEMPERATURE = 0x01,  // Установить целевую температуру
    CFG_CMD_SET_INTERVAL    = 0x02,  // Установить интервал измерений
    CFG_CMD_SET_MODE        = 0x03   // Установить режим работы
} ConfigCommand_t;

// Коды команд запроса (RequestCommand)
typedef enum {
    REQ_CMD_GET_STATUS  = 0x01,  // Запросить текущий статус устройства
    REQ_CMD_GET_VERSION = 0x02,  // Запросить версию прошивки
    REQ_CMD_GET_CONFIG  = 0x03   // Запросить текущую конфигурацию
} RequestCommand_t;

// Статусы обработки команд
typedef enum {
    CMD_STATUS_OK               = 0x00,
    CMD_STATUS_CRC_ERROR        = 0x01,
    CMD_STATUS_INVALID_TYPE     = 0x02,
    CMD_STATUS_INVALID_CODE     = 0x03,
    CMD_STATUS_INVALID_LENGTH   = 0x04,
    CMD_STATUS_EXECUTION_ERROR  = 0x05,
    CMD_STATUS_TIMEOUT          = 0x06,
    CMD_STATUS_UNKNOWN_ERROR    = 0xFF
} CommandStatus_t;

// Структура команды
typedef struct {
    uint8_t commandType;               // Тип команды
    uint8_t commandCode;               // Код команды
    uint8_t dataLength;                // Длина данных
    uint8_t data[CMD_MAX_DATA_LENGTH]; // Данные параметров
    uint16_t crc;                      // Контрольная сумма CRC16
} Command_t;

// Структура ответа на команду
typedef struct {
    uint8_t commandType;               // Тип ответа (обычно RESPONSE)
    uint8_t commandCode;               // Код исходной команды
    uint8_t status;                    // Статус выполнения
    uint8_t dataLength;                // Длина данных ответа
    uint8_t data[CMD_MAX_DATA_LENGTH]; // Данные ответа
    uint16_t crc;                      // Контрольная сумма CRC16
} CommandResponse_t;

// Глобальные функции модуля
void CommandReceiver_Init(void);
void CommandReceiver_Task(void *argument);
CommandStatus_t CommandReceiver_ProcessCommand(Command_t *cmd);
void CommandReceiver_SendResponse(CommandResponse_t *response);

// Обработчики команд по типам
CommandStatus_t CommandReceiver_HandleProgControl(Command_t *cmd);
CommandStatus_t CommandReceiver_HandleConfiguration(Command_t *cmd);
CommandStatus_t CommandReceiver_HandleRequest(Command_t *cmd);
CommandStatus_t CommandReceiver_HandleDeviceControl(Command_t *cmd);

// Вспомогательные функции
uint16_t CommandReceiver_CalculateCRC(uint8_t *data, uint16_t length);
uint8_t CommandReceiver_ValidateCRC(uint8_t *data, uint16_t length, uint16_t receivedCRC);
CommandStatus_t CommandReceiver_ReceiveCommand(Command_t *cmd);
void CommandReceiver_ProcessReceivedData(uint16_t receivedSize);
void CommandReceiver_RestartReception(void);
void CommandReceiver_OnDataReceived(uint16_t receivedSize);

#ifdef __cplusplus
extern "C" {
#endif
    void CommandReceiver_Task_C(void *argument);
#ifdef __cplusplus
}
#endif

#endif /* COMMANDRECEIVER_HPP_ */

