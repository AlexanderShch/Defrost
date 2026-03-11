# Считывание и запись параметров контроллера Defrost

Документация по командам **запроса и записи параметров** авто-дефроста: один параметр или группа параметров. Чтение — команды типа `REQUEST` (ответ с префиксом `AA 55` на линии). Запись групп 5 и 6 — команда конфигурации **CFG_CMD_SET_DEFROST_GROUP** (тип `CONFIGURATION`, код `0x05`).

**Файлы реализации:**  
Контроллер: `STM32CubeIDE/ProjectCode/CommandReceiver.cpp`, `DefrostControl.cpp`, `DefrostControl.h`.  
Сервер: `ProjectServerW` — кнопки «Считать параметры», «Записать параметры», формирование и разбор payload.

---

## 1. Общий протокол команд

- **Канал:** UART (например UART4), скорость по настройкам проекта.
- **Запрос от хоста:** кадр без префикса — `[Type][Code][DataLen][Data...][CRC16]`.  
  CRC16 считается по полю **от первого байта до конца данных** (включительно), алгоритм — ModBus CRC16.
- **Ответ контроллера:** на линию выдаётся `[AA 55][Type][Len][Code][Status][DataLen][Data...][CRC16]`.  
  `Len` — длина полезной части после `Len` до CRC (т.е. Code + Status + DataLen + Data). CRC считается по блоку `[Type][Len][Code][Status][DataLen][Data...]`.
- **Тип команд запроса:** `CommandType = 0x03` (REQUEST).

---

## 2. Запрос одного параметра (REQ_CMD_GET_DEFROST_PARAM)

**Код команды:** `0x06` (REQ_CMD_GET_DEFROST_PARAM).

### 2.1. Запрос

| Байт  | Поле      | Значение | Описание                          |
|-------|-----------|----------|-----------------------------------|
| 0     | Type      | `0x03`   | REQUEST                           |
| 1     | Code      | `0x06`   | GET_DEFROST_PARAM                 |
| 2     | DataLen   | `0x02`   | длина данных (всегда 2)            |
| 3     | Data[0]   | groupId  | идентификатор группы параметров    |
| 4     | Data[1]   | paramId  | идентификатор параметра в группе   |
| 5–6   | CRC16     | —        | младший, старший байт (ModBus)     |

**Пример запроса (hex):** запрос параметра группы 2 (температура), параметр 0:

```
03 06 02 02 00 [CRC_LO] [CRC_HI]
```

### 2.2. Ответ при успехе (Status = 0x00)

| Байт (после AA 55) | Поле      | Описание                                      |
|--------------------|-----------|-----------------------------------------------|
| 0                  | Type      | `0x03`                                        |
| 1                  | Len       | длина полезной части (3 + DataLen)             |
| 2                  | Code      | `0x06`                                        |
| 3                  | Status    | `0x00` (OK)                                   |
| 4                  | DataLen   | 4, 5 или 7 (см. ниже)                         |
| 5                  | groupId   | повтор группы из запроса                       |
| 6                  | paramId   | повтор параметра из запроса                   |
| 7                  | valueType | тип значения (1=U8, 2=U16, 3=F32)              |
| 8…                 | value     | 1 байт (U8), 2 байта (U16) или 4 байта (F32)  |

- **valueType = 1 (U8):** DataLen = 4, один байт значения в `data[3]`.
- **valueType = 2 (U16):** DataLen = 5, два байта (little-endian) в `data[3..4]`.
- **valueType = 3 (F32):** DataLen = 7, четыре байта (float, little-endian) в `data[3..6]`.

При ошибке выполнения контроллер возвращает тот же Code и Status = `0x05` (CMD_STATUS_EXECUTION_ERROR), без полезных данных.

---

## 3. Запрос группы параметров (REQ_CMD_GET_DEFROST_GROUP)

**Код команды:** `0x07` (REQ_CMD_GET_DEFROST_GROUP).

### 3.1. Запрос

| Байт  | Поле      | Значение | Описание                |
|-------|-----------|----------|-------------------------|
| 0     | Type      | `0x03`   | REQUEST                 |
| 1     | Code      | `0x07`   | GET_DEFROST_GROUP       |
| 2     | DataLen   | `0x02`   | длина данных (2)        |
| 3     | Data[0]   | groupId  | идентификатор группы    |
| 4     | Data[1]   | page     | страница (резерв, можно 0) |
| 5–6   | CRC16     | —        | ModBus CRC16            |

Поле `page` в текущей реализации не используется (все параметры группы возвращаются одним ответом).

**Пример запроса (hex):** все параметры группы 1 (датчики):

```
03 07 02 01 00 [CRC_LO] [CRC_HI]
```

### 3.2. Ответ при успехе

В данных ответа сначала два байта: `groupId`, `page`, затем **полезная нагрузка** (payload). Формат payload зависит от группы:

- **Группы 1–4** — последовательность записей TLV: каждая запись `[paramId][valueType][valueSize][value…]` (valueType: 1=U8, 2=U16, 3=F32; value — 1/2/4 байта, little-endian). Конец по DataLen.
- **Группа 5 (LOG_PHASE)** — фиксированная структура `DefrostLogPhasePayload_t` (копирование памяти, без TLV). Размер 72 байта: 6 массивов по 3 float (fishHotMax_C, fishHotRateMax_Cps, fishDeltaMax_C, supplySet_C, supplyMax_C, returnTargetRH_percent).
- **Группа 6 (LOG_GLOBAL)** — фиксированная структура `DefrostLogGlobalPayload_t` (копирование памяти, без TLV). Размер 50 байт: 6× float, 8× uint16_t, 1× float `fishColdTarget_C` (целевая мин. Т рыбы °C; при достижении алгоритм останавливается), 6× uint8_t sensorUseInDefrost (порядок полей см. в `DefrostControl.h`).

Сервер и контроллер используют одинаковые структуры; приём — один `memcpy` payload в структуру.

---

## 4. Запись группы параметров (CFG_CMD_SET_DEFROST_GROUP)

**Тип команды:** `CommandType = 0x02` (CONFIGURATION).  
**Код команды:** `0x05` (CFG_CMD_SET_DEFROST_GROUP).

Используется для **записи целиком группы 5 или группы 6** в контроллер. Payload совпадает с форматом **ответа** GET_DEFROST_GROUP для соответствующей группы: те же бинарные структуры `DefrostLogPhasePayload_t` (группа 5) и `DefrostLogGlobalPayload_t` (группа 6).

### 4.1. Формат команды

| Байт  | Поле      | Значение | Описание |
|-------|-----------|----------|----------|
| 0     | Type      | `0x02`   | CONFIGURATION |
| 1     | Code      | `0x05`   | SET_DEFROST_GROUP |
| 2     | DataLen   | 1 + размер payload | длина данных |
| 3     | Data[0]   | groupId  | идентификатор группы: **5** (LOG_PHASE) или **6** (LOG_GLOBAL) |
| 4…    | Data[1…]  | payload  | бинарные данные группы (как в ответе GET_DEFROST_GROUP для этой группы) |
| …     | CRC16     | —        | ModBus CRC16 по полям до CRC |

- **Группа 5:** payload = 72 байта (`DefrostLogPhasePayload_t`: 6×3 float — fishHotMax_C, fishHotRateMax_Cps, fishDeltaMax_C, supplySet_C, supplyMax_C, returnTargetRH_percent).
- **Группа 6:** payload = 50 байт (`DefrostLogGlobalPayload_t`: 6× float, 8× uint16_t, float fishColdTarget_C, 6× uint8_t sensorUseInDefrost; порядок полей в `DefrostControl.h`).

### 4.2. Ответ контроллера

Стандартный ответ на команду CONFIGURATION: Type=0x02, Code=0x05, Status=0x00 при успехе. При ошибке (неверная длина, неверный groupId, ошибка применения) — Status=0x04 (INVALID_LENGTH) или 0x05 (EXECUTION_ERROR).

### 4.3. Обработка на контроллере

- **CommandReceiver:** по коду `CFG_CMD_SET_DEFROST_GROUP` извлекает `groupId` из `Data[0]`, payload — из `Data[1]…Data[DataLen-1]`, вызывает `DefrostControl_SetGroupPayload(groupId, payload, payloadLen)`.
- **DefrostControl_SetGroupPayload:** для groupId 5 копирует payload в `DefrostLogPhasePayload_t` и переносит поля в `g_defrostParams`; для groupId 6 — в `DefrostLogGlobalPayload_t`, переносит в `g_defrostParams`, обновляет `Sensor_array` и глобальные переменные. После применения вызывается **DefrostControl_SaveParams()**: параметры сохраняются в статическую структуру **в RAM** (без записи во Flash); после сброса питания загружаются значения по умолчанию.

---

## 5. Группы и идентификаторы параметров

Группы задаются перечислением `DefrostParamGroup_t` в `DefrostControl.h`:

| groupId | Имя (DefrostParamGroup_t)      | Описание                                                                 |
|---------|--------------------------------|--------------------------------------------------------------------------|
| 1       | DEFROST_PARAM_GROUP_SENSORS    | Использование датчиков в дефросте (чтение/запись GET_PARAM, SET_PARAM)   |
| 2       | DEFROST_PARAM_GROUP_TEMPERATURE| Температура, ПИ, баланс лево/право (чтение/запись)                       |
| 3       | DEFROST_PARAM_GROUP_HUMIDITY   | Влажность, вытяжка, форсунка (чтение/запись)                             |
| 4       | DEFROST_PARAM_GROUP_PWM        | Тайминги, режим «только по воздуху» (чтение/запись)                      |
| 5       | DEFROST_PARAM_GROUP_LOG_PHASE  | Лог: параметры по фазам — чтение GET_DEFROST_GROUP, запись SET_DEFROST_GROUP |
| 6       | DEFROST_PARAM_GROUP_LOG_GLOBAL | Лог: общие параметры — чтение GET_DEFROST_GROUP, запись SET_DEFROST_GROUP  |

**Разделение по способу доступа:**
- **Группы 1–4** — конфигурируемые параметры: запрос одного параметра (GET_DEFROST_PARAM), запрос группы (GET_DEFROST_GROUP), запись одного параметра (SET_DEFROST_PARAM).
- **Группы 5–6** — чтение по запросу GET_DEFROST_GROUP и **запись целиком группы** по команде SET_DEFROST_GROUP (раздел 4). Используются для выгрузки и загрузки «лога» параметров (по фазам и общих) между сервером и контроллером. В регулярный пакет лога Type 0x01 они не входят.
- **Регулярный лог (Type 0x01)** передаётся контроллером автоматически и содержит: текущую фазу процесса и **переменные алгоритма** (структура `ControlLogPayload_t` — «группа 3» в смысле лога: eT_common, heatScale01, скважности ТЭНов и т.д.). Это не groupId 3 (HUMIDITY).

### 5.1. Группа 1 — Датчики (SENSORS)

| paramId | Описание                         | Тип  | Единица / примечание      |
|---------|----------------------------------|------|---------------------------|
| 0…(N−1) | Использование датчика в дефросте | U8   | 0 = не использовать, 1 = использовать |

Количество датчиков N задаётся в прошивке: `kDefrostSensorCount = min(SQ, DEFROST_MAX_SENSOR_COUNT)` (например SQ=7, DEFROST_MAX_SENSOR_COUNT=16). Допустимые paramId: 0…(N−1).

### 5.2. Группа 2 — Температура (TEMPERATURE)

| paramId | Описание                                      | Тип  | Примечание        |
|---------|-----------------------------------------------|------|-------------------|
| 0, 1, 2 | fishHotMax_C[фаза]                            | F32  | °C, фазы 0–2      |
| 3, 4, 5 | supplySet_C[фаза]                             | F32  | °C                |
| 6, 7, 8 | supplyMax_C[фаза]                             | F32  | °C                |
| 9,10,11 | fishDeltaMax_C[фаза]                          | F32  | °C                |
| 12,13,14| fishHotRateMax_Cps[фаза]                      | F32  | °C/с              |
| 15      | leftRightTrimGain                             | F32  | —                 |
| 16      | leftRightTrimMaxEq                            | F32  | —                 |
| 17      | piKp                                         | F32  | ПИ-регулятор       |
| 18      | piKi                                         | F32  | ПИ-регулятор       |

Фазы: 0 = WarmUp, 1 = Plateau, 2 = Finish.

### 5.3. Группа 3 — Влажность (HUMIDITY)

| paramId | Описание                | Тип  | Единица / примечание |
|---------|-------------------------|------|----------------------|
| 0, 1, 2 | returnTargetRH_percent[фаза] | F32 | %                    |
| 3       | wDeadband_kgkg          | F32  | кг/кг                |
| 4       | outDamperTimer_s        | U16  | с                    |
| 5       | outFanDelay_s           | U16  | с                    |
| 6       | injGain                 | F32  | —                    |

### 5.4. Группа 4 — ШИМ/тайминги (PWM)

| paramId | Описание             | Тип  | Единица |
|---------|----------------------|------|---------|
| 0       | tenMinHold_s         | U16  | с       |
| 1       | injMinHold_s         | U16  | с       |
| 2       | outHold_s            | U16  | с       |
| 3       | airOnlyPhaseWarmUp_s | U16  | с       |
| 4       | airOnlyPhasePlateau_s| U16  | с       |
| 5       | maxRuntime_s         | U16  | с       |

### 5.5. Группа 5 — Лог: параметры, зависящие от фазы (LOG_PHASE)

Чтение: **REQ_CMD_GET_DEFROST_GROUP** с groupId = 5. Запись: **CFG_CMD_SET_DEFROST_GROUP** (раздел 4) с groupId = 5 и payload = `DefrostLogPhasePayload_t`. Возвращает/принимает параметры **для всех трёх фаз** процесса (0=WarmUp, 1=Plateau, 2=Finish). В регулярный пакет лога Type 0x01 не входит.

Формат: paramId 0..5 — фаза 0, 6..11 — фаза 1, 12..17 — фаза 2. В каждой фазе 6 параметров в порядке таблицы ниже.

| paramId (в фазе) | Описание                    | Тип  | Примечание        |
|------------------|-----------------------------|------|-------------------|
| 0                | fishHotMax_C                | F32  | °C                |
| 1                | fishHotRateMax_Cps          | F32  | °C/с              |
| 2                | fishDeltaMax_C              | F32  | °C                |
| 3                | supplyMax_C                 | F32  | °C                |
| 4                | supplySet_C                 | F32  | °C                |
| 5                | returnTargetRH_percent      | F32  | %                 |

Итого: paramId 0..17 (phaseIdx = paramId/6, subId = paramId%6).

### 5.6. Группа 6 — Лог: параметры, общие для всех фаз (LOG_GLOBAL)

Чтение: **REQ_CMD_GET_DEFROST_GROUP** с groupId = 6. Запись: **CFG_CMD_SET_DEFROST_GROUP** (раздел 4) с groupId = 6 и payload = `DefrostLogGlobalPayload_t`. В регулярный пакет лога Type 0x01 не входит.

| paramId | Описание                 | Тип  | Примечание |
|---------|--------------------------|------|------------|
| 0       | leftRightTrimGain        | F32  | —          |
| 1       | leftRightTrimMaxEq       | F32  | —          |
| 2       | piKp                     | F32  | —          |
| 3       | piKi                     | F32  | —          |
| 4       | wDeadband_kgkg           | F32  | кг/кг      |
| 5       | injGain                  | F32  | —          |
| 6       | outDamperTimer_s         | U16  | с          |
| 7       | outFanDelay_s            | U16  | с          |
| 8       | outHold_s                | U16  | с          |
| 9       | tenMinHold_s             | U16  | с          |
| 10      | injMinHold_s             | U16  | с          |
| 11      | airOnlyPhaseWarmUp_s     | U16  | с          |
| 12      | airOnlyPhasePlateau_s    | U16  | с          |
| 13      | maxRuntime_s             | U16  | с          |
| 14…(14+N−1) | sensorUseInDefrost[] | U8   | по одному байту на датчик; N = DEFROST_MAX_SENSOR_COUNT (16) |

---

## 5. CRC16 (ModBus)

CRC считается по всем байтам кадра **до** поля CRC (в запросе — по [Type][Code][DataLen][Data...], в ответе — по [Type][Len][Code][Status][DataLen][Data...]). Порядок байт в CRC: сначала младший байт, затем старший.

Алгоритм (как в проекте и в Version_Command_Guide):

```python
def calculate_crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc
```

---

## 6. Процесс записи параметров в контроллер

Описывается цепочка от нажатия кнопки «Записать параметры» на сервере до обновления параметров в RAM контроллера.

### 6.1. Сервер (ProjectServerW)

1. **Кнопка «Записать параметры»** (`buttonWriteParameters`): проверяется наличие соединения и что хотя бы одна из таблиц параметров (таблица 1 — параметры по фазам, таблица 2 — общие параметры) содержит строки с данными. Если обе пусты — выводится сообщение и запись не выполняется.

2. **Формирование payload группы 5:** из таблицы 1 (dataGridView1) по строкам 0…5 в фиксированном порядке (fishHotMax_C, fishHotRateMax_Cps, fishDeltaMax_C, supplySet_C, supplyMax_C, returnTargetRH_percent) считываются значения колонок WarmUP, Plateau, Finish и собирается структура `DefrostLogPhasePayload_t`. Если в таблице есть данные, формируется команда **SET_DEFROST_GROUP** с groupId=5 и payload 72 байта; команда отправляется контроллеру, ожидается ответ со Status=OK.

3. **Формирование payload группы 6:** из таблицы 2 (dataGridView2) по имени параметра (колонка Parameter2) и значению (колонка Value) заполняется структура `DefrostLogGlobalPayload_t`. Если в таблице есть данные, формируется команда **SET_DEFROST_GROUP** с groupId=6 и payload 44 байта; команда отправляется контроллеру, ожидается ответ со Status=OK.

4. В статусной строке выводится результат: обе группы записаны, только одна, или ошибка.

### 6.2. Контроллер

1. **CommandReceiver** принимает кадр типа CONFIGURATION, код 0x05 (CFG_CMD_SET_DEFROST_GROUP). Проверяется длина данных (не менее 2: groupId + хотя бы 1 байт payload). Извлекаются groupId = Data[0], указатель на payload = &Data[1], payloadLen = DataLen − 1.

2. Вызывается **DefrostControl_SetGroupPayload(groupId, payload, payloadLen)**. Для groupId 5 проверяется размер payload ≥ sizeof(DefrostLogPhasePayload_t), данные копируются в временную структуру и переносятся в `g_defrostParams` (поля по фазам). Для groupId 6 — то же с DefrostLogGlobalPayload_t, плюс обновление `Sensor_array[i].UseInDefrost` и глобальных переменных (g.leftRightTrimGain и т.д.).

3. После успешного применения для каждой группы вызывается **DefrostControl_SaveParams()**: текущие `g_defrostParams` копируются в статический буфер **g_defrostParamsStorage** в RAM, вычисляется и сохраняется CRC. Записи во Flash или EEPROM нет; после сброса питания при следующем старте загружаются значения по умолчанию (LoadParams проверяет версию и CRC в g_defrostParamsStorage и при несовпадении вызывает LoadDefaultParams).

---

## 7. Пример: запрос одного параметра (Python)

```python
import serial
import struct

def calculate_crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def get_defrost_param(ser, group_id, param_id):
    """Запросить один параметр авто-дефроста. Возвращает (value_type, value) или None."""
    command = bytearray([0x03, 0x06, 0x02, group_id, param_id])
    crc = calculate_crc16(command)
    command.append(crc & 0xFF)
    command.append((crc >> 8) & 0xFF)
    ser.write(command)

    # Ответ: [AA 55][Type][Len][Code][Status][DataLen][Data...][CRC]
    raw = ser.read(64)
    if len(raw) < 8:
        return None
    # Пропуск префикса AA 55 при наличии
    i = 0
    if raw[0] == 0xAA and raw[1] == 0x55:
        i = 2
    if raw[i] != 0x03 or raw[i + 2] != 0x06:
        return None
    status = raw[i + 3]
    data_len = raw[i + 4]
    if status != 0x00 or data_len < 3:
        return None
    value_type = raw[i + 7]
    if value_type == 1:  # U8
        return (1, raw[i + 8])
    if value_type == 2:  # U16
        return (2, struct.unpack_from('<H', raw, i + 8)[0])
    if value_type == 3:  # F32
        return (3, struct.unpack_from('<f', raw, i + 8)[0])
    return None

# Пример: получить уставку влажности возврата для фазы 0 (группа 3, paramId 0)
# ser = serial.Serial('COM3', 19200, timeout=1)
# vt, val = get_defrost_param(ser, 3, 0)
# if vt == 3:
#     print(f"returnTargetRH_percent[0] = {val} %")
```

---

## 8. Пример: запрос группы параметров (Python)

```python
def get_defrost_group(ser, group_id, page=0):
    """Запросить все параметры группы. Возвращает список (paramId, valueType, value) или None."""
    command = bytearray([0x03, 0x07, 0x02, group_id, page])
    crc = calculate_crc16(command)
    command.append(crc & 0xFF)
    command.append((crc >> 8) & 0xFF)
    ser.write(command)

    raw = ser.read(256)
    if len(raw) < 8:
        return None
    i = 0
    if raw[0] == 0xAA and raw[1] == 0x55:
        i = 2
    if raw[i] != 0x03 or raw[i + 2] != 0x07 or raw[i + 3] != 0x00:
        return None
    data_len = raw[i + 4]
    payload = bytes(raw[i + 5 : i + 5 + data_len])
    if len(payload) < 2:
        return []
    payload = payload[2:]  # убрать groupId, page

    result = []
    pos = 0
    while pos + 3 <= len(payload):
        param_id = payload[pos]
        value_type = payload[pos + 1]
        value_size = payload[pos + 2]
        pos += 3
        if pos + value_size > len(payload):
            break
        if value_type == 1 and value_size == 1:
            result.append((param_id, 1, payload[pos]))
        elif value_type == 2 and value_size == 2:
            result.append((param_id, 2, struct.unpack_from('<H', payload, pos)[0]))
        elif value_type == 3 and value_size == 4:
            result.append((param_id, 3, struct.unpack_from('<f', payload, pos)[0]))
        pos += value_size
    return result
```

---

## 9. Коды ошибок (Status)

| Код | Константа                  | Описание                    |
|-----|----------------------------|-----------------------------|
| 0x00| CMD_STATUS_OK              | Успех                       |
| 0x01| CMD_STATUS_CRC_ERROR       | Ошибка CRC (в запросе)      |
| 0x04| CMD_STATUS_INVALID_LENGTH  | Неверная длина данных (для GET_PARAM — 2, для GET_GROUP — 2; для SET_DEFROST_GROUP — не менее 2) |
| 0x05| CMD_STATUS_EXECUTION_ERROR | Ошибка при чтении/записи параметра (неверный groupId/paramId, неверный размер payload или внутренняя ошибка) |

---

## 10. Группы параметров процесса дефростации (сводка по логу)

Используются две схемы нумерации:

1. **groupId 1–6** — идентификаторы групп в командах GET_DEFROST_PARAM / GET_DEFROST_GROUP (раздел 4).
2. **«Группы лога» 1, 2, 3** — смысловое деление данных, передаваемых серверу:
   - **Лог по запросу, группа 1** — параметры по фазам: запрос **REQ_CMD_GET_DEFROST_GROUP** с **groupId = 5** (раздел 4.5).
   - **Лог по запросу, группа 2** — общие параметры: запрос **REQ_CMD_GET_DEFROST_GROUP** с **groupId = 6** (раздел 4.6).
   - **Регулярный лог (пакет Type 0x01)** передаётся контроллером автоматически и содержит **текущую фазу** и **«группу 3»** — переменные алгоритма (структура `ControlLogPayload_t`: eT_common, heatScale01, скважности ТЭНов, влажность, fishHot_C и т.д.). Это не groupId 3 (HUMIDITY).

### 10.1. Лог по запросу: параметры, зависящие от фазы (groupId = 5)

По запросу **REQ_CMD_GET_DEFROST_GROUP**, groupId = **5**. Возвращаются параметры **для всех трёх фаз** (paramId 0..17: по 6 параметров на фазу). В регулярный пакет лога Type 0x01 не входят (там передаётся только текущая фаза в поле **phase** и переменные алгоритма — «группа 3»).

| Параметр (для каждой фазы 0,1,2) | Описание                                                                 |
|----------------------------------|--------------------------------------------------------------------------|
| fishHotMax_C                     | Потолок температуры самой тёплой точки продукта, °C                      |
| fishHotRateMax_Cps               | Потолок скорости прогрева «горячей» точки, °C/с                        |
| fishDeltaMax_C                  | Потолок разницы горячая–холодная точка продукта, °C                     |
| supplyMax_C                     | Потолок температуры воздуха в потоках подачи, °C                         |
| supplySet_C                     | Уставка температуры воздуха подачи, °C                                  |
| returnTargetRH_percent          | Уставка RH в возврате для фазы, %                                       |

### 10.2. Лог по запросу: параметры, общие для всех фаз (groupId = 6)

По запросу **REQ_CMD_GET_DEFROST_GROUP**, groupId = **6**. Не входят в регулярный пакет лога Type 0x01.

| Поле / paramId | Описание                                                                 |
|----------------|--------------------------------------------------------------------------|
| leftRightTrimGain (0) … maxRuntime_s (13) | См. таблицу в п. 4.6              |
| sensorUseInDefrost[] (14…29) | По каждому датчику: 1 = использовать в дефросте, 0 = игнорировать |

### 10.3. Регулярный лог Type 0x01: текущая фаза и переменные алгоритма («группа 3» лога)

В каждом пакете лога Type 0x01 контроллер передаёт **текущую фазу** процесса и переменные алгоритма (структура `ControlLogPayload_t`). Это не groupId 3 (HUMIDITY), а отдельный поток данных «группы 3» в смысле лога.

| Поле в логе            | Описание                                                                 |
|------------------------|--------------------------------------------------------------------------|
| **phase**              | Текущая фаза процесса (0=WarmUp, 1=Plateau, 2=Finish)                  |
| **eT_common**          | Общая ошибка по температуре (вход ПИ-регулятора)                        |
| **heatScale01**        | Масштаб нагрева 0…1                                                      |
| **uCommon_TEN**        | Общее управление ТЭНами                                                  |
| **trim_TEN**           | Подстройка ТЭН (баланс лево/право)                                       |
| **uLeft_TEN**, **uRight_TEN** | Управление левым/правым ТЭНом                                    |
| **leftTen1Duty**, **leftTen2Duty**, **rightTen1Duty**, **rightTen2Duty** | Скважности ШИМ ТЭНов |
| **w_sup_avg**          | Средняя абсолютная влажность в подаче                                    |
| **wErr**               | Ошибка по влажности                                                      |
| **injDuty**            | Скважность форсунки                                                      |
| **rate_Cps**           | Скорость прогрева «горячей» точки продукта, °C/с                        |
| **fishHot_C**, **fishCold_C** | Температуры самой тёплой и самой холодной точки продукта (°C); в режиме «только по воздуху» — 0 |

---

Документация подготовлена по коду Defrost (`CommandReceiver.cpp`, `DefrostControl.cpp`, `DefrostControl.h`) и сервера ProjectServerW. При изменении групп или идентификаторов параметров в прошивке таблицы в разделах 5.1–5.6 и 10 нужно обновить.
