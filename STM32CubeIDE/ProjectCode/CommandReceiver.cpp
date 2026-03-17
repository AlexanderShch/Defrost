/*
 * CommandReceiver.cpp
 *
 *  Создан: 23 октября 2025
 *  Автор: Система
 *  Описание: Реализация модуля приема и обработки команд от сервера
 */

#include "CommandReceiver.hpp"
#include "ModBus.hpp"
#include "Data.hpp"
#include "DefrostControl.h"
#include "version.h"
#include <string.h>
#include <gui/model/model.hpp>

// Внешние переменные/ресурсы определены в C-модулях (main.c).
// Используем C-линковку, чтобы избежать манглинга имён символов в C++ и сделать линковку детерминированной.
extern "C" {
extern UART_HandleTypeDef huart4;  // UART для связи с сервером
extern osSemaphoreId_t PR_TX_Compl_SemHandle;  // завершение передачи (UART4)
extern osSemaphoreId_t UART4_CMD_RX_SemHandle;  // завершение приёма сервера (UART4)
extern osSemaphoreId_t ServerResponseReceived_SemHandle;  // ответ на телеметрию получен (DATA_OK/DATA_FALSE)
extern SENSOR_typedef_t Sensor_array[SQ];  // массив датчиков
extern unsigned int TimeFromStart;  // время устройства в секундах
}

// Буферы для приема команд
static uint8_t RX_CMD_Buffer[CMD_MAX_LENGTH];
static uint8_t TX_Response_Buffer[CMD_MAX_LENGTH];
static volatile uint16_t RX_ReceivedSize = 0;  // Размер полученных данных

// Защита: при обработке команды приоритизируем команды над низкоприоритетной телеметрией.
static volatile uint8_t g_commandReceiverHandling = 0;

// Аудит последней команды (диагностика для сервера).
static volatile uint8_t g_lastCmdType = 0;
static volatile uint8_t g_lastCmdCode = 0;
static volatile uint16_t g_lastCmdDeviceTimeSec = 0;
static volatile uint8_t g_lastCmdAckSent = 0;
static volatile uint8_t g_lastCmdStatus = CMD_STATUS_OK;

static volatile uint8_t g_currentCmdType = 0;
static volatile uint8_t g_currentCmdCode = 0;
static volatile uint8_t g_currentCmdSkipAudit = 0;

static void WaitForUart4TxLineIdle(uint32_t timeoutMs)
{
    // Для PROG_CTRL_CMD_RESET нельзя делать сброс, пока UART ещё физически передаёт байты.
    // Это минимальное ожидание нужно, чтобы не обрезать ответ на RS-485.
    uint32_t start = osKernelGetTickCount();
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET)
    {
        if ((osKernelGetTickCount() - start) >= timeoutMs)
        {
            break;
        }
        osDelay(1);
    }
}

// Флаг: ответ на PROG_CONTROL START/STOP уже отправлен из обработчика (не дублировать в ProcessReceivedData)
static volatile uint8_t s_progControlAckSentInHandler = 0;

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
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ПРИМЕЧАНИЕ: бит _Stp больше не используется и не инициализируется
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Очистка буферов
    memset(RX_CMD_Buffer, 0, CMD_MAX_LENGTH);
    memset(TX_Response_Buffer, 0, CMD_MAX_LENGTH);
    
    // Сброс статистики
    memset(&commandStats, 0, sizeof(CommandStats_t));
    
    // Запускаем первый цикл приёма данных.
    // После получения данных и события IDLE прерывание автоматически перезапустит приём.
    // Получается непрерывный цикл: приём → IDLE → прерывание → перезапуск → приём.
    UART4_SetOwner_Server();
    HAL_UARTEx_ReceiveToIdle_DMA(&huart4, RX_CMD_Buffer, CMD_MAX_LENGTH);
    
    // Отключаем прерывание половины приема
    __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
}

/*
 * Функция: CommandReceiver_SendResponse
 * Описание: Ставит в очередь отправки на сервер ответ на команду
 * Параметры:
 *   - response: указатель на структуру ответа
 */
void CommandReceiver_SendResponse(CommandResponse_t *response)
{
    uint16_t txLength = 0;
    
    // КРИТИЧНО: Очищаем буфер перед формированием нового ответа
    // чтобы избежать отправки старых данных из предыдущего ответа
    memset(TX_Response_Buffer, 0, CMD_MAX_LENGTH);
    
    // Формируем буфер для отправки в формате:
    // [Type][Code][Status][DataLen][Data...][CRC16]
    // Где:
    // - CRC16 считается по блоку [Type][Code][Status][DataLen][Data...]
    TX_Response_Buffer[0] = response->commandType;   // тип
    TX_Response_Buffer[1] = response->commandCode;   // код
    TX_Response_Buffer[2] = response->status;        // статус
    TX_Response_Buffer[3] = response->dataLength;    // длина данных
    
    // Копируем данные
    if (response->dataLength > 0 && response->dataLength <= CMD_MAX_DATA_LENGTH)
    {
        memcpy(&TX_Response_Buffer[4], response->data, response->dataLength);
    }
    
    txLength = 4 + response->dataLength;
    
    // Вычисляем CRC
    response->crc = CommandReceiver_CalculateCRC(TX_Response_Buffer, txLength);
    TX_Response_Buffer[txLength] = response->crc & 0xFF;        // CRC (младший байт)
    TX_Response_Buffer[txLength + 1] = (response->crc >> 8) & 0xFF;  // CRC (старший байт)
    
    txLength += 2;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ОТПРАВКА через единую очередь внутреннего кадра, чтобы не конкурировать за UART с телеметрией/логом
    // Формат на линии: [AA 55][Type + Code + Status + DataLen + Data][CRC16]
    // SYNC-префикс [AA 55] добавляется позже, на этапе фактической отправки по UART,
    // в WriteToServerWithSync (в ModBus.cpp), куда доезжает item.data из очереди:
    // ═══════════════════════════════════════════════════════════════════════════
	ServerTx_EnqueueHighPriority(TX_Response_Buffer, (uint16_t)txLength);



    if (g_currentCmdSkipAudit == 0)
    {
        g_lastCmdAckSent = 1;
        g_lastCmdStatus = response->status;
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
    CommandResponse_t response;
    
    // КРИТИЧНО: Инициализируем структуру ответа
    memset(&response, 0, sizeof(CommandResponse_t));

    // Подготовка базовой структуры ответа
    // Возвращаем тот же тип команды CMD_TYPE_TELEMETRY
    response.commandType = CMD_TYPE_TELEMETRY;
    response.commandCode = cmd->commandCode;
    response.status = CMD_STATUS_OK;
    response.dataLength = 0;

    switch (cmd->commandCode)
    {
        case TELEMETRY_DATA_OK:
        {
            // Сервер подтвердил приём телеметрии
            // ✅ Успешная передача телеметрии
        	ControlLogPayload_t logPayload = {};

            // Сигнал TX_ToServer: можно продолжать (дать окно для приёма ответа перед следующим пакетом)
            if (ServerResponseReceived_SemHandle != NULL)
            {
                osSemaphoreRelease(ServerResponseReceived_SemHandle);
            }

            // Передаём лог параметров алгоритма управления в автоматическом режиме
        	logPayload = DefrostControl_GetControlLogPayload();
        	response.dataLength = (uint8_t)sizeof(logPayload);
        	memcpy(response.data, &logPayload, response.dataLength);

            CommandReceiver_SendResponse(&response);

            break;
        }
        case TELEMETRY_DATA_FALSE:
        {
            // ═══════════════════════════════════════════════════════════════════════════
            // СЕРВЕР СООБЩАЕТ ОБ ОШИБКЕ В ДАННЫХ ТЕЛЕМЕТРИИ (00 02)
            // ═══════════════════════════════════════════════════════════════════════════
            // Повторяем отправку последних данных телеметрии
            // Функция ResendLastTelemetry автоматически:
            //   - Отправляет сохранённые данные телеметрии
            //   - Инкрементирует счётчик ошибок
            ResendLastTelemetry();
            // Сигнал TX_ToServer: ответ на телеметрию получен (DATA_FALSE)
            if (ServerResponseReceived_SemHandle != NULL)
            {
                osSemaphoreRelease(ServerResponseReceived_SemHandle);
            }
            // Можно залогировать ошибку передачи
            break;
        }
            
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
        case PROG_CTRL_CMD_START: {
            // Сначала отправляем подтверждение серверу, затем запускаем процесс (чтобы сервер успел принять ACK до таймаута)
            CommandResponse_t ack;
            memset(&ack, 0, sizeof(ack));
            ack.commandType = CMD_TYPE_PROG_CONTROL;
            ack.commandCode = cmd->commandCode;
            ack.status = CMD_STATUS_OK;
            ack.dataLength = 0;
            CommandReceiver_SendResponse(&ack);
            s_progControlAckSentInHandler = 1;
            DefrostControl_SetEnabled(1);
            break;
        }
        case PROG_CTRL_CMD_STOP: {
            CommandResponse_t ack;
            memset(&ack, 0, sizeof(ack));
            ack.commandType = CMD_TYPE_PROG_CONTROL;
            ack.commandCode = cmd->commandCode;
            ack.status = CMD_STATUS_OK;
            ack.dataLength = 0;
            CommandReceiver_SendResponse(&ack);
            s_progControlAckSentInHandler = 1;
            DefrostControl_SetEnabled(0);
            break;
        }
            
        case PROG_CTRL_CMD_PAUSE:
            // Приостановка программы
            break;
            
        case PROG_CTRL_CMD_RESUME:
            // Возобновление программы
            break;
            
        case PROG_CTRL_CMD_RESET: {
            // Сначала отправляем подтверждение серверу (как для START/STOP), затем сброс — после возврата и паузы в ProcessReceivedData.
            CommandResponse_t ack;
            memset(&ack, 0, sizeof(ack));
            ack.commandType = CMD_TYPE_PROG_CONTROL;
            ack.commandCode = cmd->commandCode;
            ack.status = CMD_STATUS_OK;
            ack.dataLength = 0;
            CommandReceiver_SendResponse(&ack);
            s_progControlAckSentInHandler = 1;
            status = CMD_STATUS_OK;
            break;
        }
            
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
                uint16_t intervalSeconds = 0;
                memcpy(&intervalSeconds, cmd->data, sizeof(uint16_t));

                // Установить интервал отправки телеметрии на сервер (сек).
                Telemetry_SetIntervalSeconds(intervalSeconds);
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

        case CFG_CMD_SET_DEFROST_PARAM:
        {
            // Формат payload: [groupId][paramId][valueType][value...]
            if (cmd->dataLength < 4)
            {
                status = CMD_STATUS_INVALID_LENGTH;
                break;
            }

            DefrostParamValue_t value;
            memset(&value, 0, sizeof(value));
            const uint8_t groupId = cmd->data[0];
            const uint8_t paramId = cmd->data[1];
            value.valueType = cmd->data[2];

            if (value.valueType == DEFROST_PARAM_TYPE_U8)
            {
                if (cmd->dataLength != 4)
                {
                    status = CMD_STATUS_INVALID_LENGTH;
                    break;
                }
                value.value.u8 = cmd->data[3];
            }
            else if (value.valueType == DEFROST_PARAM_TYPE_U16)
            {
                if (cmd->dataLength != 5)
                {
                    status = CMD_STATUS_INVALID_LENGTH;
                    break;
                }
                memcpy(&value.value.u16, &cmd->data[3], sizeof(uint16_t));
            }
            else if (value.valueType == DEFROST_PARAM_TYPE_F32)
            {
                if (cmd->dataLength != 7)
                {
                    status = CMD_STATUS_INVALID_LENGTH;
                    break;
                }
                memcpy(&value.value.f32, &cmd->data[3], sizeof(float));
            }
            else
            {
                status = CMD_STATUS_INVALID_LENGTH;
                break;
            }

            if (DefrostControl_SetParam(groupId, paramId, &value) == 0u)
            {
                status = CMD_STATUS_EXECUTION_ERROR;
            }
            break;
        }

        case CFG_CMD_SET_DEFROST_GROUP:
        {
            /* Формат: data[0] = groupId, data[1..] = payload (как в ответе GET_DEFROST_GROUP). */
            if (cmd->dataLength < 2u)
            {
                status = CMD_STATUS_INVALID_LENGTH;
                break;
            }
            const uint8_t groupId = cmd->data[0];
            const uint8_t *payload = &cmd->data[1];
            const uint8_t payloadLen = cmd->dataLength - 1u;
            if (DefrostControl_SetGroupPayload(groupId, payload, payloadLen) == 0u)
            {
                status = CMD_STATUS_EXECUTION_ERROR;
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
    // Возвращаем тот же тип команды REQUEST
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
            break;
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
        
        case REQ_CMD_GET_CMD_INFO:
        {
            uint8_t lastCmdType = g_lastCmdType;
            uint8_t lastCmdCode = g_lastCmdCode;
            uint16_t lastCmdTimeSec = g_lastCmdDeviceTimeSec;
            uint8_t ackSent = g_lastCmdAckSent;
            uint8_t lastCmdStatus = g_lastCmdStatus;

            response.data[0] = lastCmdType;
            response.data[1] = lastCmdCode;
            response.data[2] = (uint8_t)(lastCmdTimeSec & 0xFF);
            response.data[3] = (uint8_t)((lastCmdTimeSec >> 8) & 0xFF);
            response.data[4] = ackSent;
            response.data[5] = lastCmdStatus;
            response.dataLength = 6;

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

        case REQ_CMD_GET_DEFROST_PARAM:
        {
            // Отправляем запрошенный параметр алгоритма дефростации
            // Запрос: [groupId][paramId]
            if (cmd->dataLength != 2)
            {
                status = CMD_STATUS_INVALID_LENGTH;
                response.status = status;
                CommandReceiver_SendResponse(&response);
                break;
            }

            DefrostParamValue_t value;
            memset(&value, 0, sizeof(value));
            const uint8_t groupId = cmd->data[0];
            const uint8_t paramId = cmd->data[1];

            if (DefrostControl_GetParam(groupId, paramId, &value) == 0u)
            {
                status = CMD_STATUS_EXECUTION_ERROR;
                response.status = status;
                CommandReceiver_SendResponse(&response);
                break;
            }

            response.data[0] = groupId;
            response.data[1] = paramId;
            response.data[2] = value.valueType;
            if (value.valueType == DEFROST_PARAM_TYPE_U8)
            {
                response.data[3] = value.value.u8;
                response.dataLength = 4;
            }
            else if (value.valueType == DEFROST_PARAM_TYPE_U16)
            {
                memcpy(&response.data[3], &value.value.u16, sizeof(uint16_t));
                response.dataLength = 5;
            }
            else if (value.valueType == DEFROST_PARAM_TYPE_F32)
            {
                memcpy(&response.data[3], &value.value.f32, sizeof(float));
                response.dataLength = 7;
            }
            else
            {
                status = CMD_STATUS_EXECUTION_ERROR;
                response.status = status;
            }

            CommandReceiver_SendResponse(&response);
            break;
        }

        case REQ_CMD_GET_DEFROST_GROUP:
        {
            // Отправляем группу параметров алгоритма дефростации
            // Запрос: [groupId][page]
            if (cmd->dataLength != 2)
            {
                status = CMD_STATUS_INVALID_LENGTH;
                response.status = status;
                CommandReceiver_SendResponse(&response);
                break;
            }

            const uint8_t groupId = cmd->data[0];
            const uint8_t page = cmd->data[1];
            uint8_t payloadLen = 0;

            response.data[0] = groupId;
            response.data[1] = page;
            if (DefrostControl_GetGroup(groupId, page, &response.data[2], CMD_MAX_DATA_LENGTH - 2, &payloadLen) == 0u)
            {
                status = CMD_STATUS_EXECUTION_ERROR;
                response.status = status;
                CommandReceiver_SendResponse(&response);
                break;
            }

            response.dataLength = (uint8_t)(payloadLen + 2);
            CommandReceiver_SendResponse(&response);
            break;
        }

        case REQ_CMD_SEND_STATE:
        {
            /* Ответом на команду являются кадры телеметрии и лога (не CommandResponse). */
        	MSGQUEUE_OBJ_t DataTelemetry = {};
        	DataTelemetry = Data_CurrentTelemetry();
        	response.dataLength = (uint8_t)sizeof(DataTelemetry);
        	memcpy(response.data, &DataTelemetry, response.dataLength);

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
    semStatus = osSemaphoreAcquire(UART4_CMD_RX_SemHandle, 1000); // 1 секунда на заголовок
    
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
        semStatus = osSemaphoreAcquire(UART4_CMD_RX_SemHandle, 2000); // 2 секунды на данные
        
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
    semStatus = osSemaphoreAcquire(UART4_CMD_RX_SemHandle, 1000); // 1 секунда на CRC
    
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
    if (CommandReceiver_CalculateCRC(RX_CMD_Buffer, totalLength) != cmd->crc)
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
    Command_t receivedCommand;      // структура для хранения команды
    CommandStatus_t cmdStatus;      // статус обработки команды
    static uint8_t localBuffer[CMD_MAX_LENGTH];  // static для экономии стека

    g_commandReceiverHandling = 1;  // устанавливаем флаг обработки команды
    g_currentCmdSkipAudit = 0;      // сбрасываем флаг пропуска аудита
    
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
        g_commandReceiverHandling = 0;
        return;
    }
    
    // Парсим заголовок команды из локального буфера
    receivedCommand.commandType = localBuffer[0];
    receivedCommand.commandCode = localBuffer[1];
    receivedCommand.dataLength = localBuffer[2];

    g_currentCmdType = receivedCommand.commandType;
    g_currentCmdCode = receivedCommand.commandCode;
    // Пропускаем аудит для:
    // 1.
    // 2. REQ_CMD_GET_CMD_INFO - чтобы запрос не затирал аудит сам по себе
    g_currentCmdSkipAudit = (receivedCommand.commandType == CMD_TYPE_REQUEST &&
                             receivedCommand.commandCode == REQ_CMD_GET_CMD_INFO) ? 1 : 0;
    
    // КРИТИЧНО: Проверяем недопустимую комбинацию Type=0x00 и Code=0x00
    // Это артефакт из-за обработки пустого буфера при ложном IDLE
    if (receivedCommand.commandType == 0x00 && receivedCommand.commandCode == 0x00)
    {
        commandStats.invalidCommands++;
        g_commandReceiverHandling = 0;
        return;  // Игнорируем фантомную команду, не отправляем ответ
    }
    
    // Проверяем корректность длины данных
    if (receivedCommand.dataLength > CMD_MAX_DATA_LENGTH)
    {
        commandStats.invalidCommands++;
        g_commandReceiverHandling = 0;
        return;
    }
    
    // Проверяем общую длину сообщения
    uint16_t expectedLength = CMD_HEADER_SIZE + receivedCommand.dataLength + CMD_CRC_SIZE;
    if (receivedSize != expectedLength)
    {
        commandStats.invalidCommands++;
        g_commandReceiverHandling = 0;
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
                    
    // Сохраняем данные для аудита последней команды
    if (g_currentCmdSkipAudit == 0)
    {
        g_lastCmdType = receivedCommand.commandType;
        g_lastCmdCode = receivedCommand.commandCode;
        g_lastCmdDeviceTimeSec = (uint16_t)TimeFromStart;
        g_lastCmdAckSent = 0;
        g_lastCmdStatus = CMD_STATUS_OK;
    }

    // Проверяем CRC
    uint16_t totalLength = CMD_HEADER_SIZE + receivedCommand.dataLength;
    if (CommandReceiver_CalculateCRC(localBuffer, totalLength) != receivedCommand.crc)
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
        g_commandReceiverHandling = 0;
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
    // PROG_CONTROL START/STOP уже отправили ответ из HandleProgControl (s_progControlAckSentInHandler)
    if (originalCommandType != CMD_TYPE_TELEMETRY &&
        originalCommandType != CMD_TYPE_REQUEST)
    {
        if (originalCommandType == CMD_TYPE_PROG_CONTROL && s_progControlAckSentInHandler != 0)
        {
            s_progControlAckSentInHandler = 0;
            // ответ уже отправлен в HandleProgControl
        }
        else if (originalCommandType == CMD_TYPE_PROG_CONTROL || 
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
        WaitForUart4TxLineIdle(50);
        // Пауза, чтобы ответ успел уйти из очереди UART и мост TCP↔UART успел переслать его серверу
        osDelay(200);
        HAL_NVIC_SystemReset();
    }

    g_commandReceiverHandling = 0;
}

uint8_t CommandReceiver_IsHandling(void)
{
    return g_commandReceiverHandling;
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
    osSemaphoreRelease(UART4_CMD_RX_SemHandle);
    
    // Перезапускаем прием для следующего сообщения
    CommandReceiver_RestartReception();
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
        // Ждем семафор от прерывания о получении данных (таймаут 100 мс)
        osStatus_t semStatus = osSemaphoreAcquire(UART4_CMD_RX_SemHandle, 100);
        
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


