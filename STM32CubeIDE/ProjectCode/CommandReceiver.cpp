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
#include "version.h"
#include <string.h>
#include <gui/model/model.hpp>

// Внешние переменные и ресурсы
extern UART_HandleTypeDef huart4;  // UART для связи с сервером
extern osSemaphoreId_t PR_RX_Compl_SemHandle;  // Семафор приема
extern osSemaphoreId_t PR_TX_Compl_SemHandle;  // Семафор передачи
extern SENSOR_typedef_t Sensor_array[SQ];  // Массив датчиков и модулей

// Буферы для приема команд
static uint8_t RX_CMD_Buffer[CMD_MAX_LENGTH];
static uint8_t TX_Response_Buffer[CMD_MAX_LENGTH];
static volatile uint16_t RX_ReceivedSize = 0;  // Размер полученных данных

// Тип функции-колбэка для действий с битом (сброс или установка)
typedef void (*BitActionCallback_t)(void);

// Структура для управления таймерами битов
typedef struct {
    volatile uint32_t* timer;        // Указатель на переменную таймера
    BitActionCallback_t action;      // Функция действия (сброс/установка бита)
    const char* name;                // Название таймера (для отладки)
} ControlBitTimer_t;

// Таймеры для битов управления (время в мс, когда нужно выполнить действие)
static volatile uint32_t WrkBitTimer = 0;  // Таймер для бита _Wrk (зелёная лампа РАБОТА)
static volatile uint32_t StpBitTimer = 0;  // Таймер для бита _Stp (красная лампа СТОП)

// Функции для управления битами (обёртки для битовых полей)
static void ResetWrkBit(void) { Model::DFR._Wrk = 0; }  // Сброс бита _Wrk
static void SetStpBit(void) { Model::DFR._Stp = 1; }    // Установка бита _Stp

// Массив таймеров для централизованной обработки
static ControlBitTimer_t controlBitTimers[] = {
    { &WrkBitTimer, ResetWrkBit, "_Wrk" },  // _Wrk: включается на 3 сек, затем сбрасывается
    { &StpBitTimer, SetStpBit, "_Stp" }     // _Stp: сбрасывается на 3 сек, затем восстанавливается
};

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
    // ═══════════════════════════════════════════════════════════════════════════
    // КРИТИЧНО: ПЕРВЫМ ДЕЙСТВИЕМ устанавливаем режим приёма (DE = 0)
    // Это гарантирует правильное состояние после любого сброса системы
    // ═══════════════════════════════════════════════════════════════════════════
    HAL_GPIO_WritePin(PROG_MASTER_DE_GPIO_Port, PROG_MASTER_DE_Pin, GPIO_PIN_RESET);
    
    // Небольшая задержка для стабилизации GPIO
    HAL_Delay(1);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Инициализация битов управления
    // ═══════════════════════════════════════════════════════════════════════════
    Model::DFR._Stp = 1;  // Бит СТОП активен по умолчанию (кнопка СТОП нормально замкнута)
    
    // Очистка буферов
    memset(RX_CMD_Buffer, 0, CMD_MAX_LENGTH);
    memset(TX_Response_Buffer, 0, CMD_MAX_LENGTH);
    
    // Сброс статистики
    memset(&commandStats, 0, sizeof(CommandStats_t));
    
    // Запускаем первый цикл приема данных
    // После получения данных и IDLE, прерывание автоматически перезапустит прием
    // Это создает непрерывный цикл: прием → IDLE → прерывание → перезапуск → прием...
    HAL_UARTEx_ReceiveToIdle_DMA(&huart4, RX_CMD_Buffer, CMD_MAX_LENGTH);
    
    // Отключаем прерывание половины приема
    __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
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
    
    // КРИТИЧНО: Очищаем буфер перед формированием нового ответа
    // чтобы избежать отправки старых данных из предыдущего ответа
    memset(TX_Response_Buffer, 0, CMD_MAX_LENGTH);
    
    // Формируем буфер для отправки
    // КРИТИЧНО: Записываем значения из структуры response в буфер
    TX_Response_Buffer[0] = response->commandType;   // Type
    TX_Response_Buffer[1] = response->commandCode;   // Code
    TX_Response_Buffer[2] = response->status;        // Status
    TX_Response_Buffer[3] = response->dataLength;    // DataLen
    
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
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Проверяем состояние UART4 перед передачей (RS-485 Half Duplex)
    // ═══════════════════════════════════════════════════════════════════════════
    HAL_UART_StateTypeDef uartState = HAL_UART_GetState(&huart4);
    
    // Проверяем TX канал - если занят, ЖДЁМ завершения передачи через семафор
    if ((uartState & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX)
    {
        // Ждём семафор завершения передачи (его выдаст прерывание HAL_UART_TxCpltCallback)
        osStatus_t semStatus = osSemaphoreAcquire(PR_TX_Compl_SemHandle, 100);
        
        if (semStatus != osOK)
        {
            // Не дождались завершения передачи
            return;
        }
    }
    
    // Проверяем RX канал и ждём завершения активного приёма (если он идёт)
    MB_Error_t rxCheckResult = CheckAndWaitForActiveReception(&huart4, &PR_RX_Compl_SemHandle);
    if (rxCheckResult != MB_ERROR_NO)
    {
        // Не удалось дождаться завершения приёма - отменяем передачу
        return;
    }
    
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
 * Функция: CommandReceiver_HandleTelemetry
 * Описание: Обработчик ответов сервера на телеметрию (TELEMETRY)
 * Параметры:
 *   - cmd: указатель на структуру команды
 * Возвращает: статус выполнения команды
 */
CommandStatus_t CommandReceiver_HandleTelemetry(Command_t *cmd)
{
    CommandStatus_t status = CMD_STATUS_OK;
    
    switch (cmd->commandCode)
    {
        case TELEMETRY_DATA_OK:
            // Сервер подтвердил приём телеметрии
            // TODO: Можно сбросить счётчик ошибок передачи
            // TODO: Обновить статус связи с сервером
            // TODO: Залогировать успешную передачу
            break;
            
        case TELEMETRY_DATA_FALSE:
            // Сервер сообщает об ошибке в данных телеметрии
            // TODO: Повторить отправку телеметрии
            // TODO: Инкрементировать счётчик ошибок
            // TODO: Залогировать ошибку передачи
            break;
            
        default:
            status = CMD_STATUS_INVALID_CODE;
            break;
    }
    
    return status;
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
            // Проверка: модуль ввода-вывода (SQ=6) должен быть активен
            if (Sensor_array[6].Active != 1)
            {
                // Модуль ввода-вывода не активен - прерываем исполнение команды
                status = CMD_STATUS_EXECUTION_ERROR;
                break;
            }
            
            // Активируем автоматический режим работы
            Model::Flag_DFR_manual = 0;  // Автоматический режим
            
            // Устанавливаем бит _Wrk (зелёная лампа РАБОТА) на 3 секунды
            Model::DFR._Wrk = 1;
            WrkBitTimer = osKernelGetTickCount() + 3000;  // Сбросить через 3000 мс
            break;
            
        case PROG_CTRL_CMD_STOP:
            // Остановка программы
            // Проверка: модуль ввода-вывода (SQ=6) должен быть активен
            if (Sensor_array[6].Active != 1)
            {
                // Модуль ввода-вывода не активен - прерываем исполнение команды
                status = CMD_STATUS_EXECUTION_ERROR;
                break;
            }
            
            // СБРАСЫВАЕМ бит _Stp (красная лампа СТОП гаснет) на 3 секунды
            // После окончания таймера бит автоматически вернется в 1 (лампа загорится снова)
            Model::DFR._Stp = 0;
            StpBitTimer = osKernelGetTickCount() + 3000;  // Восстановить в 1 через 3000 мс
            break;
            
        case PROG_CTRL_CMD_PAUSE:
            // Приостановка программы
            break;
            
        case PROG_CTRL_CMD_RESUME:
            // Возобновление программы
            break;
            
        case PROG_CTRL_CMD_RESET:
            // Сбрасываем устройство
            // ВАЖНО: Отправляем ответ сервису ПЕРЕД сбросом
            // Сброс выполняется после возврата из функции ответа серверу
            status = CMD_STATUS_OK;
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
            {}
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
                // float targetTemp;
                // memcpy(&targetTemp, cmd->data, sizeof(float));
                
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
                // uint16_t interval;
                // memcpy(&interval, cmd->data, sizeof(uint16_t));
                
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
    
    // КРИТИЧНО: Инициализируем структуру ответа
    memset(&response, 0, sizeof(CommandResponse_t));
    
    // Подготовка базовой структуры ответа
    // Возвращаем тот же тип команды REQUEST, а не CMD_TYPE_RESPONSE
    response.commandType = CMD_TYPE_REQUEST;
    response.commandCode = cmd->commandCode;
    response.status = CMD_STATUS_OK;
    response.dataLength = 0;
    
    switch (cmd->commandCode)
    {
        case REQ_CMD_GET_STATUS:
        {
            // Отправляем текущий статус устройства
            // Например, состояние регистров DFR
//            uint16_t currentStatus = Model::DFR_current;
//            memcpy(response.data, &currentStatus, sizeof(uint16_t));
//            response.dataLength = sizeof(uint16_t);
//
//            // Отправляем ответ
//            CommandReceiver_SendResponse(&response);
//            break;
        }
        
        case REQ_CMD_GET_VERSION:
        {
            // Отправляем версию прошивки из version.h
            // Формат ответа: MAJOR.MINOR.PATCH (строка)
            const char *version = FW_VERSION_STRING;
            uint8_t versionLen = strlen(version);
            
            if (versionLen <= CMD_MAX_DATA_LENGTH)
            {
                memcpy(response.data, version, versionLen);
                response.dataLength = versionLen;
            }
            else
            {
                // Версия слишком длинная - отправляем ошибку
                status = CMD_STATUS_INVALID_LENGTH;
                response.status = status;
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
        
        case REQ_CMD_GET_BUILD_INFO:
        {
            // Отправляем полную информацию о сборке
            // Формат: "v1.0.0 (Oct 25 2025 14:30:45)"
            const char *buildInfo = FW_VERSION_FULL;
            uint8_t buildInfoLen = strlen(buildInfo);
            
            if (buildInfoLen <= CMD_MAX_DATA_LENGTH)
            {
                memcpy(response.data, buildInfo, buildInfoLen);
                response.dataLength = buildInfoLen;
            }
            else
            {
                // Информация слишком длинная - отправляем только версию
                const char *version = FW_VERSION_STRING;
                uint8_t versionLen = strlen(version);
                memcpy(response.data, version, versionLen);
                response.dataLength = versionLen;
            }
            
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
        case CMD_TYPE_TELEMETRY:
            status = CommandReceiver_HandleTelemetry(cmd);
            break;
            
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
 * Функция: CommandReceiver_ReceiveCommand
 * Описание: Надежный прием команды от сервера с проверкой целостности
 * Параметры:
 *   - cmd: указатель на структуру команды для заполнения
 * Возвращает: статус приема команды
 */
CommandStatus_t CommandReceiver_ReceiveCommand(Command_t *cmd)
{
    HAL_StatusTypeDef halStatus;
    osStatus_t semStatus;
    
        // Очистка буфера приема
        memset(RX_CMD_Buffer, 0, CMD_MAX_LENGTH);
        
    // Сначала принимаем заголовок команды (3 байта)
    halStatus = HAL_UART_Receive_DMA(&huart4, RX_CMD_Buffer, CMD_HEADER_SIZE);
    
    if (halStatus != HAL_OK)
    {
        return CMD_STATUS_TIMEOUT;
    }
    
    // Ждем получения заголовка
    semStatus = osSemaphoreAcquire(PR_RX_Compl_SemHandle, 1000); // 1 секунда на заголовок
    
    if (semStatus != osOK)
    {
        HAL_UART_AbortReceive_IT(&huart4);
        return CMD_STATUS_TIMEOUT;
    }
    
    // Парсим заголовок
    cmd->commandType = RX_CMD_Buffer[0];
    cmd->commandCode = RX_CMD_Buffer[1];
    cmd->dataLength = RX_CMD_Buffer[2];
    
    // Проверяем корректность длины данных
    if (cmd->dataLength > CMD_MAX_DATA_LENGTH)
    {
        HAL_UART_AbortReceive_IT(&huart4);
        return CMD_STATUS_INVALID_LENGTH;
    }
    
    // Если есть данные, принимаем их
    if (cmd->dataLength > 0)
    {
        halStatus = HAL_UART_Receive_DMA(&huart4, &RX_CMD_Buffer[CMD_HEADER_SIZE], cmd->dataLength);
        
        if (halStatus != HAL_OK)
        {
            return CMD_STATUS_TIMEOUT;
        }
        
        // Ждем получения данных
        semStatus = osSemaphoreAcquire(PR_RX_Compl_SemHandle, 2000); // 2 секунды на данные
        
        if (semStatus != osOK)
        {
            HAL_UART_AbortReceive_IT(&huart4);
            return CMD_STATUS_TIMEOUT;
        }
        
        // Копируем данные в структуру команды
        memcpy(cmd->data, &RX_CMD_Buffer[CMD_HEADER_SIZE], cmd->dataLength);
    }
    
    // Принимаем CRC (2 байта)
    halStatus = HAL_UART_Receive_DMA(&huart4, &RX_CMD_Buffer[CMD_HEADER_SIZE + cmd->dataLength], CMD_CRC_SIZE);
    
    if (halStatus != HAL_OK)
    {
        return CMD_STATUS_TIMEOUT;
    }
    
    // Ждем получения CRC
    semStatus = osSemaphoreAcquire(PR_RX_Compl_SemHandle, 1000); // 1 секунда на CRC
    
    if (semStatus != osOK)
    {
        HAL_UART_AbortReceive_IT(&huart4);
        return CMD_STATUS_TIMEOUT;
    }
    
    // Извлекаем CRC
    cmd->crc = RX_CMD_Buffer[CMD_HEADER_SIZE + cmd->dataLength] | 
               (RX_CMD_Buffer[CMD_HEADER_SIZE + cmd->dataLength + 1] << 8);
    
    // Проверяем CRC
    uint16_t totalLength = CMD_HEADER_SIZE + cmd->dataLength;
    if (!CommandReceiver_ValidateCRC(RX_CMD_Buffer, totalLength, cmd->crc))
    {
        return CMD_STATUS_CRC_ERROR;
    }
    
    return CMD_STATUS_OK;
}

/*
 * Функция: CommandReceiver_ProcessReceivedData
 * Описание: Обработка полученных данных из прерывания UART
 * Параметры:
 *   - receivedSize: количество полученных байт
 */
void CommandReceiver_ProcessReceivedData(uint16_t receivedSize)
{
    Command_t receivedCommand;
    CommandStatus_t cmdStatus;
    static uint8_t localBuffer[CMD_MAX_LENGTH];  // Static для экономии стека
    
    // КРИТИЧНО: Инициализируем структуру команды нулями
    // чтобы избежать случайных значений в неиспользуемых полях
    memset(&receivedCommand, 0, sizeof(Command_t));
    
    // КРИТИЧНО: Копируем данные из RX_CMD_Buffer в локальный буфер
    // чтобы защититься от перезаписи при следующем приеме
    memcpy(localBuffer, RX_CMD_Buffer, receivedSize);
    
    // Сразу очищаем буфер приема, чтобы минимизировать риск потери данных
    // при быстром следующем приеме (DMA уже перезапущен в прерывании)
    memset(RX_CMD_Buffer, 0, CMD_MAX_LENGTH);
    
    // Увеличиваем счетчик полученных команд
    commandStats.totalCommandsReceived++;
    
    // Проверяем минимальную длину сообщения
    if (receivedSize < CMD_HEADER_SIZE + CMD_CRC_SIZE)
    {
        commandStats.invalidCommands++;
        return;
    }
    
    // Парсим заголовок команды из локального буфера
    receivedCommand.commandType = localBuffer[0];
    receivedCommand.commandCode = localBuffer[1];
    receivedCommand.dataLength = localBuffer[2];
    
    // Проверяем корректность длины данных
    if (receivedCommand.dataLength > CMD_MAX_DATA_LENGTH)
    {
        commandStats.invalidCommands++;
        return;
    }
    
    // Проверяем общую длину сообщения
    uint16_t expectedLength = CMD_HEADER_SIZE + receivedCommand.dataLength + CMD_CRC_SIZE;
    if (receivedSize != expectedLength)
    {
        commandStats.invalidCommands++;
        return;
    }
    // Копируем данные
    if (receivedCommand.dataLength > 0)
    {
        memcpy(receivedCommand.data, &localBuffer[CMD_HEADER_SIZE], receivedCommand.dataLength);
    }
    // Извлекаем CRC
    receivedCommand.crc = localBuffer[CMD_HEADER_SIZE + receivedCommand.dataLength] | 
                          (localBuffer[CMD_HEADER_SIZE + receivedCommand.dataLength + 1] << 8);
                    
    // Проверяем CRC
    uint16_t totalLength = CMD_HEADER_SIZE + receivedCommand.dataLength;
    if (!CommandReceiver_ValidateCRC(localBuffer, totalLength, receivedCommand.crc))
    {
        commandStats.crcErrors++;
        
        // Отправляем ответ об ошибке CRC (с тем же типом команды)
        CommandResponse_t response;
        memset(&response, 0, sizeof(CommandResponse_t));  // Инициализируем структуру
        response.commandType = receivedCommand.commandType;  // Возвращаем тот же тип команды
        response.commandCode = receivedCommand.commandCode;
        response.status = CMD_STATUS_CRC_ERROR;
        response.dataLength = 0;
        
        CommandReceiver_SendResponse(&response);
        return;
    }
    
    // КРИТИЧНО: Сохраняем тип и код команды ДО обработки
    // так как обработчики могут модифицировать структуру receivedCommand
    uint8_t originalCommandType = receivedCommand.commandType;
    uint8_t originalCommandCode = receivedCommand.commandCode;
    
    // Обрабатываем команду
    cmdStatus = CommandReceiver_ProcessCommand(&receivedCommand);
    
    // Для команд, требующих подтверждения, отправляем ответ
    // TELEMETRY (0x00) не требует ответа, так как это уже ответ от сервера
    // REQUEST (0x03) не требует ответа здесь, так как отправляет свой ответ внутри обработчика
    if (originalCommandType != CMD_TYPE_TELEMETRY &&
        originalCommandType != CMD_TYPE_REQUEST)
    {
        // Отправляем подтверждение для PROG_CONTROL, DEVICE_CONTROL, CONFIGURATION
        if (originalCommandType == CMD_TYPE_PROG_CONTROL || 
            originalCommandType == CMD_TYPE_DEVICE_CONTROL ||
            originalCommandType == CMD_TYPE_CONFIGURATION)
        {
            CommandResponse_t response;
            memset(&response, 0, sizeof(CommandResponse_t));  // Инициализируем структуру
            response.commandType = originalCommandType;  // Используем СОХРАНЁННЫЙ тип команды
            response.commandCode = originalCommandCode;  // Используем СОХРАНЁННЫЙ код команды
            response.status = cmdStatus;
            response.dataLength = 0;
            
            CommandReceiver_SendResponse(&response);
        }
    }
    
    // КРИТИЧНО: Проверяем команду сброса ПОСЛЕ отправки ответа
    // Используем оригинальные значения команды
    if (originalCommandType == CMD_TYPE_PROG_CONTROL && 
        originalCommandCode == PROG_CTRL_CMD_RESET &&
        cmdStatus == CMD_STATUS_OK)
    {
        // Даём время на завершение передачи ответа
        osDelay(100);
        // Теперь выполняем сброс
        HAL_NVIC_SystemReset();
    }
}

/*
 * Функция: CommandReceiver_RestartReception
 * Описание: Перезапускает прием данных (вызывается из прерывания)
 *           Создает непрерывный цикл приема
 */
void CommandReceiver_RestartReception(void)
{
    // ВАЖНО: Не очищаем буфер здесь!
    // Очистка будет в потоке CommandReceiver_Task после копирования данных
    
    // Перезапускаем прием - ждем следующего сообщения до IDLE
    HAL_UARTEx_ReceiveToIdle_DMA(&huart4, RX_CMD_Buffer, CMD_MAX_LENGTH);
    __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
}

/*
 * Функция: CommandReceiver_OnDataReceived
 * Описание: Вызывается из прерывания при получении данных
 * Параметры:
 *   - receivedSize: количество полученных байт
 */
void CommandReceiver_OnDataReceived(uint16_t receivedSize)
{
    // Сохраняем размер полученных данных
    RX_ReceivedSize = receivedSize;
    
    // Освобождаем семафор для уведомления потока
    osSemaphoreRelease(PR_RX_Compl_SemHandle);
    
    // Перезапускаем прием для следующего сообщения
    CommandReceiver_RestartReception();
}

/*
 * Функция: CommandReceiver_ProcessControlBitTimers
 * Описание: Проверяет и сбрасывает все биты управления по таймерам
 * 
 * Эта универсальная функция обрабатывает все таймеры битов управления
 * (например, _Wrk, _Stp и т.д.) в едином цикле.
 * Для добавления нового таймера достаточно добавить запись в массив controlBitTimers.
 */
static void CommandReceiver_ProcessControlBitTimers(void)
{
    uint32_t currentTick = osKernelGetTickCount();
    
    // Количество таймеров в массиве
    const size_t timerCount = sizeof(controlBitTimers) / sizeof(controlBitTimers[0]);
    
    // Проходим по всем таймерам
    for (size_t i = 0; i < timerCount; i++)
    {
        // Проверяем, установлен ли таймер
        if (*(controlBitTimers[i].timer) > 0)
        {
            // Если время истекло, выполняем действие (сброс или установка)
            if (currentTick >= *(controlBitTimers[i].timer))
            {
                controlBitTimers[i].action();           // Вызываем функцию действия
                *(controlBitTimers[i].timer) = 0;       // Отключаем таймер
            }
        }
    }
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
    
    // Основной цикл задачи - ожидаем получения данных и обрабатываем их
    while (1)
    {
        // Проверяем все таймеры битов управления (_Wrk, _Stp, и т.д.)
        CommandReceiver_ProcessControlBitTimers();
        
        // Ждем семафор от прерывания о получении данных (с таймаутом для проверки таймеров)
        osStatus_t semStatus = osSemaphoreAcquire(PR_RX_Compl_SemHandle, 100);
        
        if (semStatus == osOK && RX_ReceivedSize > 0)
        {
            // Обрабатываем полученные данные
            // (функция копирует данные в локальный буфер и сразу очищает RX_CMD_Buffer)
            CommandReceiver_ProcessReceivedData(RX_ReceivedSize);
            
            // Сбрасываем размер
            RX_ReceivedSize = 0;
        }
    }
}

/*
 * Обертка для вызова из C
 */
extern "C" void CommandReceiver_Task_C(void *argument)
{
    CommandReceiver_Task(argument);
}


