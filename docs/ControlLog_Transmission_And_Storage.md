# Передача и сохранение лога параметров алгоритма

Документ описывает цепочку формирования, передачи и сохранения пакетов лога алгоритма управления разморозкой (Type 0x01): от контроллера Defrost до записи в CSV на сервере ProjectServerW.

---

## 1. Обзор

- **Контроллер** раз в секунду (интервал задаётся `LOG_INTERVAL_DEFAULT_SEC`) формирует пакет с текущим снимком параметров алгоритма, ставит его в единую очередь отправки на сервер и отправляет по UART в формате `[AA 55][Type][Len][Payload][CRC16]`.
- **Сервер** принимает кадр по TCP, распознаёт Type=0x01, кладёт пакет в очередь обработки и в потоке обработки передаёт его форме для добавления строки в CSV-файл. Контроллеру по логу подтверждение НЕ отправляется (в отличие от телеметрии).

---

## 2. Формат пакета на линии

Единый формат кадра (как для телеметрии, так и для лога):

| Часть       | Размер   | Описание |
|-------------|----------|----------|
| Синхромаркер| 2 байта  | `0xAA 0x55` |
| Type        | 1 байт   | Для лога алгоритма: **0x01** |
| Len         | 1 байт   | Длина полезной части после Len и до CRC (в байтах). Для лога: `sizeof(ControlLogPayload_t)` = **65** байт |
| Payload     | Len байт | Type 0x01: `ControlLogPayload_t` (см. ниже) |
| CRC16       | 2 байта  | CRC по блоку `[Type][Len][Payload]` (ModBus CRC-16) |

Размер пакета лога на линии: `2 + 2 + 65 + 2` = 71 байт (с маркером AA 55 и CRC). Без маркера: **LOG_PACKET_SIZE** в коде контроллера = 2 + 65 + 2 байт.

**Группировка параметров процесса** (по фазам и общие для всех фаз) описана в документе **GetParams_Command_Guide.md** (разделы 4, 9): конфигурируемые группы 1–4, лог по запросу — groupId 5 (параметры по фазам), groupId 6 (общие параметры). В регулярный пакет Type 0x01 входят только текущая фаза и переменные алгоритма ниже («группа 3» лога).

---

## 3. Структура полезной нагрузки (ControlLogPayload_t)

Определена в **DefrostControl.h** (контроллер) и должна совпадать по памяти с определением на сервере (DataForm.cpp). Размер: **65** байт.

```c
typedef struct __attribute__((packed)) {
    uint8_t phase;               /* 0=WarmUp, 1=Plateau, 2=Finish */
    float eT_common, heatScale01;
    float uCommon_TEN, trim_TEN, uLeft_TEN, uRight_TEN;
    float leftTen1Duty, leftTen2Duty, rightTen1Duty, rightTen2Duty;
    float w_sup_avg, wErr, injDuty;
    float rate_Cps;
    float fishHot_C, fishCold_C;
} ControlLogPayload_t;
```

На сервере константа **CONTROL_LOG_PAYLOAD_LEN** = 65 (SServer.cpp).

### 3.1. Перечень полей (порядок передачи)

Поля передаются в порядке объявления. Типы: `uint8_t` — 1 байт, `float` — 4 байта.

| №  | Поле            | Тип   | Описание |
|----|-----------------|-------|----------|
| 1  | `phase`         | uint8_t | Текущая фаза: 0 = WarmUp, 1 = Plateau, 2 = Finish. |
| 2  | `eT_common`     | float | Общая ошибка по температуре (вход регулятора), °C. |
| 3  | `heatScale01`   | float | Масштаб нагрева 0…1 (доля мощности). |
| 4  | `uCommon_TEN`   | float | Базовый запрос мощности ТЭНов с учётом ограничителя. |
| 5  | `trim_TEN`      | float | Коррекция лево/право по разности температур подачи (эквивалент ТЭНа). |
| 6  | `uLeft_TEN`     | float | Управление ТЭНами левой стороны (0…1). |
| 7  | `uRight_TEN`    | float | Управление ТЭНами правой стороны (0…1). |
| 8  | `leftTen1Duty`  | float | Скважность ТЭНа 1 левый, 0…1. |
| 9  | `leftTen2Duty`  | float | Скважность ТЭНа 2 левый, 0…1. |
| 10 | `rightTen1Duty` | float | Скважность ТЭНа 1 правый, 0…1. |
| 11 | `rightTen2Duty` | float | Скважность ТЭНа 2 правый, 0…1. |
| 12 | `w_sup_avg`     | float | Средняя абсолютная влажность воздуха подачи, кг/кг. |
| 13 | `wErr`          | float | Ошибка по влажности (для регулятора увлажнения), кг/кг. |
| 14 | `injDuty`       | float | Скважность форсунки увлажнения, 0…1. |
| 15 | `rate_Cps`      | float | Скорость прогрева «горячей» точки продукта, °C/с (при отсутствии замера — 0). |
| 16 | `fishHot_C`     | float | Температура самой тёплой точки продукта, °C (в режиме «только по воздуху» — 0). |
| 17 | `fishCold_C`    | float | Температура самой холодной точки продукта, °C (в режиме «только по воздуху» — 0). |

---

## 4. Контроллер (Defrost): функции и поток данных

### 4.1. Заполнение снимка параметров алгоритма

Алгоритм управления каждый шаг 1 с обновляет внутренний снимок **s_controlLogPayload** в **DefrostControl.cpp**:

- В **ControlStep1s** (режим с датчиками продукта) и в **ControlStep1s_AirOnly** (режим «только по воздуху») в конце шага заполняются поля:
  - `runtimeSeconds`, `phase`;
  - ошибка по температуре и масштаб нагрева (`eT_common`, `heatScale01`);
  - управления ТЭНами и скважности (uCommon_TEN, uLeft_TEN, uRight_TEN, leftTen1Duty … rightTen2Duty);
  - влажность (средняя, целевая, ошибка, скважность форсунки);
  - действующие лимиты по Т (Limits), температуры продукта (fishHot_C, fishCold_C), уставка T подачи (supplySet_C); влажность передаётся как w_sup_avg, w_ret_target, wErr, injDuty (returnTargetRH_percent в лог не входит).

Ниже перечислены только функции, непосредственно относящиеся к **передаче и постановке лога в очередь**.

---

### 4.2. Функции контроллера

| Файл | Функция / сущность | Описание |
|------|--------------------|----------|
| **DefrostControl.h** | `ControlLogPayload_t` | Структура полезной нагрузки лога (пакет для CSV). |
| **DefrostControl.h** | `void DefrostControl_GetControlLogPayload(ControlLogPayload_t *out, uint16_t timeFromStart)` | Копирует текущий снимок `s_controlLogPayload` в `*out` и подставляет в поле `Time` значение `timeFromStart` (секунды с включения). |
| **DefrostControl.cpp** | `DefrostControl_GetControlLogPayload(...)` | Реализация: `memcpy(out, &s_controlLogPayload, sizeof(ControlLogPayload_t)); out->Time = timeFromStart;` |
| **Data.cpp** | `#define LOG_PACKET_SIZE` | `2u + sizeof(ControlLogPayload_t) + 2u` — размер пакета без синхромаркера (Type, Len, Payload, CRC16). |
| **Data.hpp** | `LOG_INTERVAL_DEFAULT_SEC` | Интервал отправки лога в секундах (по умолчанию 1 с). Лог и телеметрия отправляются в разных циклах. |
| **Data.cpp** | `DataTimerFunc()` | Callback таймера 1 Гц. Уменьшает `LogShiftCounter`; при достижении 0 выставляет `LogSendPending = 1` и сбрасывает счётчик в `LOG_INTERVAL_DEFAULT_SEC`. |
| **Data.cpp** | `ReadDataFunc()` | Задача, пробуждаемая по флагу раз в секунду. Если `LogSendPending != 0`: формирует пакет лога (Type=0x01, Len, Payload, CRC16) в локальный буфер, заполняет `ServerTxItem_t` типом `SERVER_TX_TYPE_LOG` и кладёт элемент в очередь `Data_QueueHandle` через постановку в очередь (телеметрия и лог используют одну очередь отправки, разные типы элементов). |
| **Data.cpp** | `ServerTx_EnqueueNormal(ServerTxType_t type, const uint8_t* data, uint16_t length)` | Кладёт в очередь `Data_QueueHandle` элемент с заданным типом (для лога — `SERVER_TX_TYPE_LOG`) и копией данных; используется для телеметрии и лога. Лог в очередь явно ставится из `ReadDataFunc` (формирование пакета и вызов постановки в очередь там же). |
| **Data.cpp** | `TX_ToServer()` | Единственный поток, который забирает элементы из очередей (`osMessageQueueGet`): в каждом случае **данные берутся из очереди** — в `item` попадают `type`, `length` и копия пакета в `item.data`. Для **телеметрии** перед отправкой в `item.data` пересчитывается CRC и копия сохраняется в `LastSentTelemetry`; затем вызывается `WriteToServerWithSync(item.data, len)`. Для **лога** пакет уже полный (Type, Len, Payload, CRC был собран в `ReadDataFunc`), поэтому `item.data` и `item.length` просто передаются в `WriteToServerWithSync` без изменений. Префикс AA 55 добавляется в ModBus. |
| **ModBus.cpp** | `WriteToServerWithSync(uint8_t* Data, int length)` | Добавляет в начало буфера синхромаркер 0xAA 0x55, копирует в буфер UART и выполняет передачу по RS-485 (с ожиданием завершения приёма команд и захватом мьютекса UART4). |

Цепочка по времени: **DataTimerFunc** (1 Гц) → выставляет **LogSendPending** → **ReadDataFunc** при следующем пробуждении видит флаг, вызывает **DefrostControl_GetControlLogPayload**, собирает пакет, ставит в очередь → **TX_ToServer** забирает элемент и вызывает **WriteToServerWithSync**.

---

## 5. Сервер (ProjectServerW): функции и поток данных

### 5.1. Приём и распознавание кадра

Поток приёма в **SServer.cpp** в цикле читает данные из сокета в накопительный буфер, ищет синхромаркер `AA 55`, затем разбирает кадр:

- Читает `Type` и `Len`, проверяет длину и CRC.
- Для **Type == 0x01** и **Len == CONTROL_LOG_PAYLOAD_LEN** (65) пакет считается пакетом лога алгоритма; полезная часть (включая Type, Len, Payload, CRC) копируется в массив и передаётся в очередь обработки.

### 5.2. Функции и методы сервера

| Файл | Функция / метод | Описание |
|------|------------------|----------|
| **SServer.cpp** | `CONTROL_LOG_PAYLOAD_LEN` | Константа 65 — ожидаемая длина полезной части лога (должна совпадать с `sizeof(ControlLogPayload_t)` на контроллере). |
| **SServer.cpp** | Обработка кадра (цикл приёма) | При `framedType == 0x01` и `payloadLenByte == CONTROL_LOG_PAYLOAD_LEN` копирует кадр (payload с Type/Len + CRC) в `logBuffer` и вызывает `PacketQueueProcessor::EnqueueControlLog(...)`. |
| **PacketQueueProcessor.cpp** | `static void EnqueueControlLog(cli::array<System::Byte>^ packet, int size, int port, System::String^ formGuid, SOCKET clientSocket, System::String^ clientIP)` | Формирует элемент очереди с `itemType = 1` (control log), кладёт его в общую очередь `s_telemetryQueue` и подаёт сигнал потоку обработки. |
| **PacketQueueProcessor.cpp** | Поток обработки очереди | Для элемента с `itemType == 1` находит форму по `formGuid` и вызывает `form->AppendControlLogToCsv(item->packet, item->size)`. Подтверждение контроллеру по логу не отправляется. |
| **DataForm.h** | `void AppendControlLogToCsv(cli::array<System::Byte>^ packet, int size);` | Объявление метода записи пакета лога в CSV. |
| **DataForm.cpp** | `DataForm::AppendControlLogToCsv(...)` | Проверяет размер пакета и байт длины; копирует полезную нагрузку (начиная с `raw + 2`) в структуру `ControlLogPayload_t`; под блокировкой `controlLogSync` при первом вызове создаёт файл `log_yyyy-MM-dd_HH-mm-ss.csv` в каталоге приложения с заголовком CSV и дописывает одну строку со всеми полями структуры (Time, runtimeSeconds, phase, флаги, температуры, управления, влажность). Кодировка файла — UTF-8. |

Структура **ControlLogPayload_t** на сервере должна совпадать по полям и размерам с контроллером (см. блок в DataForm.cpp / DataForm-Samsung-NB.cpp с комментарием «Пакет лога алгоритма (Type 0x01)»).

---

## 6. Сводка цепочки

1. **Контроллер:** таймер 1 Гц → `LogSendPending` раз в `LOG_INTERVAL_DEFAULT_SEC` → в задаче ReadData: `DefrostControl_GetControlLogPayload` → сборка пакета [Type=0x01][Len][Payload][CRC16] → постановка в очередь `Data_QueueHandle` (тип `SERVER_TX_TYPE_LOG`) → поток `TX_ToServer` забирает элемент и вызывает `WriteToServerWithSync` → по UART уходит кадр [AA 55][Type][Len][Payload][CRC16].
2. **Сервер:** приём в SServer → распознавание Type=0x01 и Len=65 → `EnqueueControlLog` → очередь → поток обработки вызывает `AppendControlLogToCsv` / `AppendControlLogToDataRow` на форме → создаётся/дополняется CSV и строка в таблице данных.

Изменение полей или размера **ControlLogPayload_t** на контроллере требует синхронного изменения структуры и, при необходимости, **CONTROL_LOG_PAYLOAD_LEN** и заголовка CSV на сервере.
