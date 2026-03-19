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
| DataLen| **89**   | Размер `ControlLogPayload_t` (packed), см. ниже. |
| Data   | 89 байт  | `ControlLogPayload_t`. |

Источник на контроллере: `CommandReceiver_HandleTelemetry` → при `TELEMETRY_DATA_OK` формируется ответ с `memcpy` лога в `response.data`, вызывается `CommandReceiver_SendResponse`.

---

## 2. Структура `ControlLogPayload_t`

Определение: **DefrostControl.h** (контроллер), зеркально — **DataForm.cpp** на сервере (`#pragma pack(1)`).

Размер структуры: **89 байт** (проверяется в коде `static_assert(sizeof(ControlLogPayload_t) == 89)` на обеих сторонах).

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
} ControlLogPayload_t;
```

Заполнение снимка: **DefrostControl.cpp** (`ControlStep1s` / `ControlStep1s_AirOnly`) → `s_controlLogPayload`; чтение для отправки: `DefrostControl_GetControlLogPayload()`.

---

## 3. Связь с телеметрией

1. По команде сервера (запрос состояния) контроллер отправляет **телеметрию** кадром:
   - `Type=0x03`, `Code=0x08`, `Status=0x00`, `DataLen=45`, `Data` = `MSGQUEUE_OBJ_t`.
2. Сервер проверяет CRC, при успехе шлёт подтверждение **`DATA_OK`** в формате команды сервера: `[Type][Code][DataLen][Data][CRC]` (без префикса AA 55 на стороне TCP), см. `PacketQueueProcessor::TrySendTelemetryAck` / `CreateTelemetryAckCommand`.
3. Контроллер в `CommandReceiver_HandleTelemetry` при **TELEMETRY_DATA_OK** формирует ответ с **логом** (кадр выше, 89 байт данных).

Без успешного приёма телеметрии и DATA_OK регулярный лог этим путём не отправляется.

---

## 4. Сервер: приём и запись в таблицу

- **SServer.cpp**: поиск `AA 55`, разбор `[Type][Code][Status][DataLen][Data][CRC16]`; для лога проверяется  
  `Type==0x00 && Code==0x01 && Status==0x00 && DataLen==89` (`CONTROL_LOG_DATA_LEN`).
- **PacketQueueProcessor**: очередь, затем в UI-потоке **DataForm::AppendControlLogToDataRow** — проверка `DataLen`, `memcpy` в `ControlLogPayload_t`, колонки таблицы **T_filt_0…T_filt_5** (шесть значений), далее поля алгоритма.

---

## 5. Ссылки на код

| Сторона   | Файл | Назначение |
|-----------|------|------------|
| Контроллер | `CommandReceiver.cpp` | Ответ логом на `TELEMETRY_DATA_OK`. |
| Контроллер | `CommandReceiver.cpp` | `CommandReceiver_SendResponse` — формат полезной части + AA 55 на передаче. |
| Контроллер | `DefrostControl.h` / `DefrostControl.cpp` | Структура и заполнение лога. |
| Сервер    | `SServer.cpp` | Разбор кадра, константы 45 / 89. |
| Сервер    | `DataForm.cpp` | `ControlLogPayload_t`, `AppendControlLogToDataRow`. |

Подробнее общий протокол кадров и полудуплекс на сервере: **ProjectServerW/Doc/Отправка команд контроллеру/ПРИЁМ_И_ОБРАБОТКА_ПАКЕТОВ.md**.
