# Передача и сохранение лога параметров алгоритма

Документ описывает **текущий** контракт обмена: лог группы 3 (`ControlLogPayload_t`) от контроллера Defrost к серверу ProjectServerW по TCP (через прозрачный канал UART↔TCP).

Устаревшее описание вида «отдельный пакет Type=0x01 с полем Len» **не используется** — лог уходит **внутри фрейма ответа** с маркером `AA 55`, в том же формате, что и остальные ответы контроллера.

---

## 1. Формат кадра на линии (контроллер → сервер)

Полный кадр:

```
[AA 55][Type][Code][Status][DataLen][Data…][CRC16]
```

- **AA 55** — синхромаркер начала кадра.
- Далее идёт **полезная часть** длиной `4 + DataLen + 2` байт:
  - **Type, Code, Status, DataLen** — по 1 байту.
  - **Data** — `DataLen` байт.
  - **CRC16** — 2 байта, ModBus CRC-16 по блоку **`[Type][Code][Status][DataLen][Data…]`** (как `MB_GetCRC` на контроллере / `CalculateCommandCRC` на сервере).

### Кадр регулярного лога алгоритма

| Поле   | Значение | Пояснение |
|--------|----------|-----------|
| Type   | **0x00** | Тип «телеметрия» в терминах `CMD_TYPE_TELEMETRY` (ответ на подтверждение приёма телеметрии). |
| Code   | **0x01** | `TELEMETRY_DATA_OK` — контекст: сервер прислал DATA_OK, контроллер отвечает логом. |
| Status | **0x00** | |
| DataLen| **113**   | Размер `ControlLogPayload_t` (packed), см. ниже. |
| Data   | 113 байт  | `ControlLogPayload_t`. |

Источник на контроллере: `CommandReceiver_HandleTelemetry` → при `TELEMETRY_DATA_OK` формируется ответ с `memcpy` лога в `response.data`, вызывается `CommandReceiver_SendResponse`.

---

## 2. Структура `ControlLogPayload_t`

Определение: **DefrostControl.h** (контроллер), зеркально — **DataForm.cpp** на сервере (`#pragma pack(1)`).

Размер структуры: **113 байт**. Сервер принимает также 89, 105 и 109 байт. Поля, отсутствующие в прежнем формате, остаются пустыми в таблице.

Порядок полей (как в памяти):

```c
typedef struct __attribute__((packed)) {
    float T_filt_C[6];   /* индексы 0..5 — шесть датчиков, °C (усреднённые, param 4) */
    uint8_t phase;       /* 0=WarmUp, 1=Plateau, 2=Finish */
    float eT_common, heatScale01;
    float uCommon_TEN, trim_TEN, uLeft_TEN, uRight_TEN;
    float leftTen1Duty, leftTen2Duty, rightTen1Duty, rightTen2Duty;
    float w_sup_avg, wErr, injDuty;
    float rate_Cps;
    float fishHot_C, fishCold_C;
    float pidIntegral, pidDerivative, pidFeedforward, pidWaterDuty;
    float pidProportional;
    float supplyAverage_C;
} ControlLogPayload_t;
```

Заполнение снимка: **DefrostControl.cpp** (`ControlStep1s` / `ControlStep1s_AirOnly`) → `s_controlLogPayload`; чтение для отправки: `DefrostControl_GetControlLogPayload()`.

---

## 3. Связь с телеметрией

1. По команде сервера (запрос состояния) контроллер отправляет **телеметрию** кадром:
   - `Type=0x03`, `Code=0x08`, `Status=0x00`, `DataLen=61` для SEND_STATE с параметром 01. Первые 60 байт занимает `MSGQUEUE_OBJ_t`, а бит 0 последнего байта означает наличие событий. Ответ без расширения имеет DataLen=60.
2. Сервер проверяет CRC, при успехе шлёт подтверждение **`DATA_OK`** в формате команды сервера: `[Type][Code][DataLen][Data][CRC]` (без префикса AA 55 на стороне TCP), см. `PacketQueueProcessor::TrySendTelemetryAck` / `CreateTelemetryAckCommand`.
3. Контроллер в `CommandReceiver_HandleTelemetry` при **TELEMETRY_DATA_OK** в авторежиме формирует ответ с **логом** (кадр выше, 113 байт данных).

Без успешного приёма телеметрии и DATA_OK регулярный лог этим путём не отправляется.

---

## 4. Сервер: приём и запись в таблицу

- **SServer.cpp**: поиск `AA 55`, разбор `[Type][Code][Status][DataLen][Data][CRC16]`; для лога проверяется  
  `Type==0x00 && Code==0x01 && Status==0x00`, с DataLen 113, 109, 105 или 89.
- **PacketQueueProcessor**: очередь, затем в UI-потоке **DataForm::AppendControlLogToDataRow** — проверка `DataLen`, `memcpy` в `ControlLogPayload_t`, колонки таблицы **T_filt_0…T_filt_5** (шесть значений), далее поля алгоритма.

---

## 5. Ссылки на код

| Сторона   | Файл | Назначение |
|-----------|------|------------|
| Контроллер | `CommandReceiver.cpp` | Ответ логом на `TELEMETRY_DATA_OK`. |
| Контроллер | `CommandReceiver.cpp` | `CommandReceiver_SendResponse` — формат полезной части + AA 55 на передаче. |
| Контроллер | `DefrostControl.h` / `DefrostControl.cpp` | Структура и заполнение лога. |
| Сервер    | `SServer.cpp` | Разбор кадра, размеры телеметрии и лога. |
| Сервер    | `DataForm.cpp` | `ControlLogPayload_t`, `AppendControlLogToDataRow`. |

Подробнее общий протокол кадров и полудуплекс на сервере: **ProjectServerW/Doc/Отправка команд контроллеру/Система команд управления устройством.md**.


## Текущий протокол 23.11.2026: отдельный запрос параметров

Автоматическая отправка ControlLog после DATA_OK удалена. SEND_STATE возвращает телеметрию и признак доступности снимка параметров активного алгоритма (бит 1 расширенного байта). Признак фиксируется по DefrostControl_IsEnabled. DATA_OK лишь подтверждает получение.

Сервер запрашивает GET_PROCESS_LOG (03/0F) с новым 64-битным requestId и с ID нужной телеметрии в данных команды. Контроллер выдаёт сохранённый для неё снимок 113 байт только при совпадающем ID и активном алгоритме на момент снимка. Ответ дополняет именно её строку таблицы сервера. В простое параметры не запрашиваются.

Формат с requestId: старший бит Type равен 1, в начале Data расположен uint64 little-endian ID; DataLen учитывает дополнительные 8 байт. CRC охватывает все байты. Ответ повторяет ID запроса. ACK/NACK телеметрии повторяет ID SEND_STATE; ACK другого запроса не завершает текущее ожидание. Максимальный кадр процесса — 129 байт с синхропрефиксом, TX-элемент/UART — 132 байта. Сервер и прошивку нужно обновить вместе; новый сервер не принимает ответы без ID.
