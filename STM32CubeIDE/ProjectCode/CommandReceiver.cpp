/*
 * CommandReceiver.cpp
 *
 *  Created on: October 23, 2025
 *      Author: System
 *  Description: Реализация модуля приема и обработки команд от сервера
 */

#include "CommandReceiver.hpp"
#include "ModBus.hpp"
#include "Data.hpp"
#include <string.h>
#include <gui/model/model.hpp>

// Внешние переменные и ресурсы
extern UART_HandleTypeDef huart4;  // UART для связи с сервером
extern osSemaphoreId_t PR_RX_Compl_SemHandle;  // Семафор приема
extern osSemaphoreId_t PR_TX_Compl_SemHandle;  // Семафор передачи

// Буферы для приема команд
static uint8_t RX_CMD_Buffer[CMD_MAX_LENGTH];
static uint8_t TX_Response_Buffer[CMD_MAX_LENGTH];

// Статистика работы модуля
typedef struct {
    uint32_t totalCommandsReceived;
    uint32_t commandsProcessedOK;
    uint32_t crcErrors;
    uint32_t invalidCommands;
    uint32_t executionErrors;
} CommandStats_t;

static CommandStats_t commandStats = {0};

/*
 * CRC16 таблица для быстрого вычисления (ModBus CRC16)
 * Используем ту же таблицу, что и в ModBus
 */
extern const uint16_t crc16_table[];

/*
 * Функция: CommandReceiver_CalculateCRC
 * Описание: Вычисляет CRC16 для массива данных
 * Параметры:
 *   - data: указатель на данные
 *   - length: длина данных в байтах
 * Возвращает: значение CRC16
 */
uint16_t CommandReceiver_CalculateCRC(uint8_t *data, uint16_t length)
{
    uint16_t crc_16 = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc_16 = (crc_16 >> 8) ^ crc16_table[(data[i] ^ crc_16) & 0xFF];
    }
    return crc_16;
}

/*
 * Функция: CommandReceiver_ValidateCRC
 * Описание: Проверяет CRC16 полученных данных
 * Параметры:
 *   - data: указатель на данные (без CRC)
 *   - length: длина данных (без CRC)
 *   - receivedCRC: полученная контрольная сумма
 * Возвращает: 1 если CRC корректна, 0 если нет
 */
uint8_t CommandReceiver_ValidateCRC(uint8_t *data, uint16_t length, uint16_t receivedCRC)
{
    uint16_t calculatedCRC = CommandReceiver_CalculateCRC(data, length);
    return (calculatedCRC == receivedCRC) ? 1 : 0;
}

/*
 * Функция: CommandReceiver_Init
 * Описание: Инициализация модуля приема команд
 */
void CommandReceiver_Init(void)
{
    // Очистка буферов
    memset(RX_CMD_Buffer, 0, CMD_MAX_LENGTH);
    memset(TX_Response_Buffer, 0, CMD_MAX_LENGTH);
    
    // Сброс статистики
    memset(&commandStats, 0, sizeof(CommandStats_t));
    
    // Инициализация UART уже выполнена в main.c
}

/*
 * Функция: CommandReceiver_SendResponse
 * Описание: Отправляет ответ на команду серверу
 * Параметры:
 *   - response: указатель на структуру ответа
 */
void CommandReceiver_SendResponse(CommandResponse_t *response)
{
    uint16_t txLength = 0;
    
    // Формируем буфер для отправки
    TX_Response_Buffer[0] = response->commandType;
    TX_Response_Buffer[1] = response->commandCode;
    TX_Response_Buffer[2] = response->status;
    TX_Response_Buffer[3] = response->dataLength;
    
    // Копируем данные
    if (response->dataLength > 0 && response->dataLength <= CMD_MAX_DATA_LENGTH)
    {
        memcpy(&TX_Response_Buffer[4], response->data, response->dataLength);
    }
    
    txLength = 4 + response->dataLength;
    
    // Вычисляем CRC
    response->crc = CommandReceiver_CalculateCRC(TX_Response_Buffer, txLength);
    TX_Response_Buffer[txLength] = response->crc & 0xFF;        // CRC Lo
    TX_Response_Buffer[txLength + 1] = (response->crc >> 8) & 0xFF;  // CRC Hi
    
    txLength += 2;
    
    // Отправляем через UART4
    // Включаем направление передачи
    HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_SET);
    osDelay(1);  // Задержка перед стартовым битом
    
    HAL_StatusTypeDef result = HAL_UART_Transmit_DMA(&huart4, TX_Response_Buffer, txLength);
    
    if (result == HAL_OK)
    {
        // Ждем завершения передачи
        osSemaphoreAcquire(PR_TX_Compl_SemHandle, 100);
    }
}

/*
 * Функция: CommandReceiver_HandleProgControl
 * Описание: Обработчик команд управления программой (PROG_CONTROL)
 * Параметры:
 *   - cmd: указатель на структуру команды
 * Возвращает: статус выполнения команды
 */
CommandStatus_t CommandReceiver_HandleProgControl(Command_t *cmd)
{
    CommandStatus_t status = CMD_STATUS_OK;
    
    switch (cmd->commandCode)
    {
        case PROG_CTRL_CMD_START:
            // Запуск программы
            // Активируем автоматический режим работы
            Model::Flag_DFR_manual = 0;  // Автоматический режим
            break;
            
        case PROG_CTRL_CMD_STOP:
            // Остановка программы
            // Переключаем в ручной режим и выключаем все
            Model::Flag_DFR_manual = 1;  // Ручной режим
            Model::DFR_manual.Ten1_Left = 0;
            Model::DFR_manual.Ten2_Left = 0;
            Model::DFR_manual.Ten1_Right = 0;
            Model::DFR_manual.Ten2_Right = 0;
            break;
            
        case PROG_CTRL_CMD_PAUSE:
            // Приостановка программы
            // Можно сохранить текущее состояние и деактивировать
            Model::Flag_DFR_manual = 1;  // Переключаем в ручной режим
            break;
            
        case PROG_CTRL_CMD_RESUME:
            // Возобновление программы
            // Восстановить автоматический режим
            Model::Flag_DFR_manual = 0;  // Автоматический режим
            break;
            
        case PROG_CTRL_CMD_RESET:
            // Сброс программы (программный reset)
            // Можно использовать HAL_NVIC_SystemReset() если нужен полный сброс
            // или сбросить только параметры работы
            break;
            
        default:
            status = CMD_STATUS_INVALID_CODE;
            break;
    }
    
    return status;
}

/*
 * Функция: CommandReceiver_HandleDeviceControl
 * Описание: Обработчик команд управления устройствами (DEVICE_CONTROL)
 * Параметры:
 *   - cmd: указатель на структуру команды
 * Возвращает: статус выполнения команды
 */
CommandStatus_t CommandReceiver_HandleDeviceControl(Command_t *cmd)
{
    CommandStatus_t status = CMD_STATUS_OK;
    
    switch (cmd->commandCode)
    {
        case DEV_CTRL_CMD_RELAY_ON:
            // Включить реле (ожидаем 1 байт - номер реле)
            if (cmd->dataLength == 1)
            {
                uint8_t relayNum = cmd->data[0];
                if (relayNum < 16)
                {
                    // Устанавливаем бит в регистре ручного управления
                    Model::Flag_DFR_manual = 1;  // Ручной режим
                    uint16_t *pDFR_manual = (uint16_t*) &Model::DFR_manual;
                    *pDFR_manual |= (1 << relayNum);
                }
                else
                {
                    status = CMD_STATUS_INVALID_LENGTH;
                }
            }
            else
            {
                status = CMD_STATUS_INVALID_LENGTH;
            }
            break;
            
        case DEV_CTRL_CMD_RELAY_OFF:
            // Выключить реле (ожидаем 1 байт - номер реле)
            if (cmd->dataLength == 1)
            {
                uint8_t relayNum = cmd->data[0];
                if (relayNum < 16)
                {
                    // Сбрасываем бит в регистре ручного управления
                    Model::Flag_DFR_manual = 1;  // Ручной режим
                    uint16_t *pDFR_manual = (uint16_t*) &Model::DFR_manual;
                    *pDFR_manual &= ~(1 << relayNum);
                }
                else
                {
                    status = CMD_STATUS_INVALID_LENGTH;
                }
            }
            else
            {
                status = CMD_STATUS_INVALID_LENGTH;
            }
            break;
            
        case DEV_CTRL_CMD_RELAY_SET:
            // Установить состояние всех реле (ожидаем 2 байта - битовая маска)
            if (cmd->dataLength == 2)
            {
                uint16_t relayMask;
                memcpy(&relayMask, cmd->data, sizeof(uint16_t));
                
                // Устанавливаем регистр ручного управления
                Model::Flag_DFR_manual = 1;  // Ручной режим
                uint16_t *pDFR_manual = (uint16_t*) &Model::DFR_manual;
                *pDFR_manual = relayMask;
            }
            else
            {
                status = CMD_STATUS_INVALID_LENGTH;
            }
            break;
            
        case DEV_CTRL_CMD_HEATER_ON:
            // Включить нагреватели (ТЭНы)
            Model::Flag_DFR_manual = 1;  // Ручной режим
            Model::DFR_manual.Ten1_Left = 1;
            Model::DFR_manual.Ten2_Left = 1;
            Model::DFR_manual.Ten1_Right = 1;
            Model::DFR_manual.Ten2_Right = 1;
            break;
            
        case DEV_CTRL_CMD_HEATER_OFF:
            // Выключить нагреватели (ТЭНы)
            Model::Flag_DFR_manual = 1;  // Ручной режим
            Model::DFR_manual.Ten1_Left = 0;
            Model::DFR_manual.Ten2_Left = 0;
            Model::DFR_manual.Ten1_Right = 0;
            Model::DFR_manual.Ten2_Right = 0;
            break;
            
        case DEV_CTRL_CMD_FAN_ON:
            // Включить вентиляторы
            Model::Flag_DFR_manual = 1;  // Ручной режим
            Model::DFR_manual.Fan1_Left = 1;
            Model::DFR_manual.Fan2_Left = 1;
            Model::DFR_manual.Fan1_Right = 1;
            Model::DFR_manual.Fan2_Right = 1;
            break;
            
        case DEV_CTRL_CMD_FAN_OFF:
            // Выключить вентиляторы
            Model::Flag_DFR_manual = 1;  // Ручной режим
            Model::DFR_manual.Fan1_Left = 0;
            Model::DFR_manual.Fan2_Left = 0;
            Model::DFR_manual.Fan1_Right = 0;
            Model::DFR_manual.Fan2_Right = 0;
            break;
            
        default:
            status = CMD_STATUS_INVALID_CODE;
            break;
    }
    
    return status;
}

/*
 * Функция: CommandReceiver_HandleConfiguration
 * Описание: Обработчик команд конфигурации (CONFIGURATION)
 * Параметры:
 *   - cmd: указатель на структуру команды
 * Возвращает: статус выполнения команды
 */
CommandStatus_t CommandReceiver_HandleConfiguration(Command_t *cmd)
{
    CommandStatus_t status = CMD_STATUS_OK;
    
    switch (cmd->commandCode)
    {
        case CFG_CMD_SET_TEMPERATURE:
        {
            // Ожидаем 4 байта с float значением температуры
            if (cmd->dataLength == 4)
            {
                float targetTemp;
                memcpy(&targetTemp, cmd->data, sizeof(float));
                
                // Здесь установить целевую температуру в системе
                // Например, можно добавить переменную в Model или DFR
                // Model::TargetTemperature = targetTemp;
            }
            else
            {
                status = CMD_STATUS_INVALID_LENGTH;
            }
            break;
        }
        
        case CFG_CMD_SET_INTERVAL:
        {
            // Ожидаем 2 байта с uint16_t значением интервала в секундах
            if (cmd->dataLength == 2)
            {
                uint16_t interval;
                memcpy(&interval, cmd->data, sizeof(uint16_t));
                
                // Установить интервал измерений
                // Можно изменить DataRead_ShiftCounter
            }
            else
            {
                status = CMD_STATUS_INVALID_LENGTH;
            }
            break;
        }
        
        case CFG_CMD_SET_MODE:
        {
            // Ожидаем 1 байт с режимом работы
            if (cmd->dataLength == 1)
            {
                uint8_t mode = cmd->data[0];
                
                // Установить режим работы
                // 0 - автоматический, 1 - ручной, и т.д.
                if (mode == 0)
                {
                    Model::Flag_DFR_manual = 0;  // Автоматический режим
                }
                else if (mode == 1)
                {
                    Model::Flag_DFR_manual = 1;  // Ручной режим
                }
            }
            else
            {
                status = CMD_STATUS_INVALID_LENGTH;
            }
            break;
        }
        
        default:
            status = CMD_STATUS_INVALID_CODE;
            break;
    }
    
    return status;
}

/*
 * Функция: CommandReceiver_HandleRequest
 * Описание: Обработчик команд запроса данных (REQUEST)
 * Параметры:
 *   - cmd: указатель на структуру команды
 * Возвращает: статус выполнения команды
 */
CommandStatus_t CommandReceiver_HandleRequest(Command_t *cmd)
{
    CommandStatus_t status = CMD_STATUS_OK;
    CommandResponse_t response;
    
    // Подготовка базовой структуры ответа
    response.commandType = CMD_TYPE_RESPONSE;
    response.commandCode = cmd->commandCode;
    response.status = CMD_STATUS_OK;
    response.dataLength = 0;
    memset(response.data, 0, CMD_MAX_DATA_LENGTH);
    
    switch (cmd->commandCode)
    {
        case REQ_CMD_GET_STATUS:
        {
            // Отправляем текущий статус устройства
            // Например, состояние регистров DFR
            uint16_t currentStatus = Model::DFR_current;
            memcpy(response.data, &currentStatus, sizeof(uint16_t));
            response.dataLength = sizeof(uint16_t);
            
            // Отправляем ответ
            CommandReceiver_SendResponse(&response);
            break;
        }
        
        case REQ_CMD_GET_VERSION:
        {
            // Отправляем версию прошивки
            const char *version = "v1.0.0";
            uint8_t versionLen = strlen(version);
            if (versionLen <= CMD_MAX_DATA_LENGTH)
            {
                memcpy(response.data, version, versionLen);
                response.dataLength = versionLen;
            }
            
            // Отправляем ответ
            CommandReceiver_SendResponse(&response);
            break;
        }
        
        case REQ_CMD_GET_CONFIG:
        {
            // Отправляем текущую конфигурацию
            // Например, режим работы
            response.data[0] = Model::Flag_DFR_manual;
            response.dataLength = 1;
            
            // Отправляем ответ
            CommandReceiver_SendResponse(&response);
            break;
        }
        
        default:
            status = CMD_STATUS_INVALID_CODE;
            response.status = status;
            CommandReceiver_SendResponse(&response);
            break;
    }
    
    return status;
}

/*
 * Функция: CommandReceiver_ProcessCommand
 * Описание: Обработка полученной команды
 * Параметры:
 *   - cmd: указатель на структуру команды
 * Возвращает: статус обработки команды
 */
CommandStatus_t CommandReceiver_ProcessCommand(Command_t *cmd)
{
    CommandStatus_t status = CMD_STATUS_OK;
    
    commandStats.totalCommandsReceived++;
    
    // Обработка команды по типу
    switch (cmd->commandType)
    {
        case CMD_TYPE_PROG_CONTROL:
            status = CommandReceiver_HandleProgControl(cmd);
            break;
            
        case CMD_TYPE_CONFIGURATION:
            status = CommandReceiver_HandleConfiguration(cmd);
            break;
            
        case CMD_TYPE_REQUEST:
            status = CommandReceiver_HandleRequest(cmd);
            break;
            
        case CMD_TYPE_DEVICE_CONTROL:
            status = CommandReceiver_HandleDeviceControl(cmd);
            break;
            
        default:
            status = CMD_STATUS_INVALID_TYPE;
            commandStats.invalidCommands++;
            break;
    }
    
    // Обновление статистики
    if (status == CMD_STATUS_OK)
    {
        commandStats.commandsProcessedOK++;
    }
    else if (status == CMD_STATUS_CRC_ERROR)
    {
        commandStats.crcErrors++;
    }
    else if (status != CMD_STATUS_OK)
    {
        commandStats.executionErrors++;
    }
    
    return status;
}

/*
 * Функция: CommandReceiver_Task
 * Описание: Основная задача приема команд от сервера
 * Параметры:
 *   - argument: параметр задачи (не используется)
 */
void CommandReceiver_Task(void *argument)
{
    CommandReceiver_Init();
    
    HAL_StatusTypeDef halStatus;
    Command_t receivedCommand;
    
    // Бесконечный цикл задачи приема команд
    while (1)
    {
        // Очистка буфера приема
        memset(RX_CMD_Buffer, 0, CMD_MAX_LENGTH);
        
        // Включаем направление приема
        HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_RESET);
        
        // Запускаем прием по UART4 с ожиданием IDLE
        halStatus = HAL_UARTEx_ReceiveToIdle_DMA(&huart4, RX_CMD_Buffer, CMD_MAX_LENGTH);
        
        // Отключаем прерывание половины приема
        __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
        
        if (halStatus == HAL_OK)
        {
            // Ждем завершения приема (семафор освобождается в прерывании)
            osStatus_t semStatus = osSemaphoreAcquire(PR_RX_Compl_SemHandle, 5000);
            
            if (semStatus == osOK)
            {
                // Команда получена, парсим ее
                receivedCommand.commandType = RX_CMD_Buffer[0];
                receivedCommand.commandCode = RX_CMD_Buffer[1];
                receivedCommand.dataLength = RX_CMD_Buffer[2];
                
                // Проверка корректности длины данных
                if (receivedCommand.dataLength <= CMD_MAX_DATA_LENGTH)
                {
                    // Копируем данные
                    if (receivedCommand.dataLength > 0)
                    {
                        memcpy(receivedCommand.data, &RX_CMD_Buffer[3], receivedCommand.dataLength);
                    }
                    
                    // Извлекаем CRC (последние 2 байта)
                    uint16_t totalLength = CMD_HEADER_SIZE + receivedCommand.dataLength;
                    receivedCommand.crc = RX_CMD_Buffer[totalLength] | (RX_CMD_Buffer[totalLength + 1] << 8);
                    
                    // Проверяем CRC
                    if (CommandReceiver_ValidateCRC(RX_CMD_Buffer, totalLength, receivedCommand.crc))
                    {
                        // CRC корректна, обрабатываем команду
                        CommandStatus_t cmdStatus = CommandReceiver_ProcessCommand(&receivedCommand);
                        
                        // Для команд PROG_CONTROL, DEVICE_CONTROL и CONFIGURATION отправляем подтверждение
                        if (receivedCommand.commandType == CMD_TYPE_PROG_CONTROL || 
                            receivedCommand.commandType == CMD_TYPE_DEVICE_CONTROL ||
                            receivedCommand.commandType == CMD_TYPE_CONFIGURATION)
                        {
                            CommandResponse_t response;
                            response.commandType = CMD_TYPE_RESPONSE;
                            response.commandCode = receivedCommand.commandCode;
                            response.status = cmdStatus;
                            response.dataLength = 0;
                            
                            CommandReceiver_SendResponse(&response);
                        }
                    }
                    else
                    {
                        // Ошибка CRC
                        commandStats.crcErrors++;
                        
                        // Отправляем ответ об ошибке CRC
                        CommandResponse_t response;
                        response.commandType = CMD_TYPE_RESPONSE;
                        response.commandCode = receivedCommand.commandCode;
                        response.status = CMD_STATUS_CRC_ERROR;
                        response.dataLength = 0;
                        
                        CommandReceiver_SendResponse(&response);
                    }
                }
                else
                {
                    // Неверная длина данных
                    commandStats.invalidCommands++;
                }
            }
            else
            {
                // Таймаут приема - прерываем прием
                HAL_UART_AbortReceive_IT(&huart4);
            }
        }
        else
        {
            // Ошибка запуска приема
            osDelay(100);  // Небольшая задержка перед повторной попыткой
        }
        
        // Небольшая задержка между циклами приема
        osDelay(10);
    }
}

/*
 * Обертка для вызова из C
 */
extern "C" void CommandReceiver_Task_C(void *argument)
{
    CommandReceiver_Task(argument);
}

