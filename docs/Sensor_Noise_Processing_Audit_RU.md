# Аудит обработки шума датчиков (T) в Defrost

Документ фиксирует текущее состояние реализации по требованиям к шумозащите датчиков температуры и использованию данных в алгоритме/телеметрии.

Кодировка: UTF-8.

Связанные файлы:
- `STM32CubeIDE/ProjectCode/Data.cpp`
- `STM32CubeIDE/ProjectCode/Data.hpp`
- `STM32CubeIDE/ProjectCode/DefrostControl.cpp`
- `STM32CubeIDE/ProjectCode/CommandReceiver.cpp`

---

## 1) Матрица соответствия требованиям

| Требование | Статус | Где реализовано |
|---|---|---|
| Алгоритм обрезки шума с датчика T (`> 1.1 °C/с`) | Выполнено | `ClampRawTemperatureVsPrevClamped()` в `Data.cpp` |
| Цифровой фильтр: приведение T к средней по буферу обрезанных T | Выполнено | `SetAverageTemperature()` + `T_Clamped`/`T_Average` в `Data.cpp` |
| В расчётах управления использовать отфильтрованные T | Выполнено | `FilteredSensorT_Deci()` и `ComputeDefrostAirControlInputs()` в `DefrostControl.cpp` |
| На сервер: в телеметрии сырые T, в логе отфильтрованные | Выполнено | `Data_CurrentTelemetry()` (Param 2) + `s_controlLogPayload.T_filt_C` (Param 4) |
| Критические шумящие датчики защёлкивать в регистре и исключать из алгоритма | Выполнено | `EvaluateClampAlarmForSensor()` + проверки `SensorExcludedByClampAlarm()` |
| Наличие аварийных датчиков по ИЛИ поднимает общий флаг АВАРИЯ | Выполнено | `Model::Device_Alarm = Gate_Alarm || Device_AlarmFlags || Sensor_AlarmFlags` |

---

## 2) Подробно по цепочке обработки T

### 2.1. Сырая температура

Сырая температура с шины хранится как `Param 2`:
- запись: `Sensor::PutData(..., Param=2, ...)`
- чтение для телеметрии/экрана: `Sensor::GetData(..., Param=2)`

### 2.2. Антиспайк (обрезка скорости изменения)

В `ClampRawTemperatureVsPrevClamped()`:
- используется порог `maxDeltaDeci = 11` (десятые градуса), то есть `1.1 °C` за шаг;
- признак срабатывания (`hit=1`) ставится при `delta > 11` или `delta < -11`;
- выход ограничивается до диапазона `[-11; +11]` относительно предыдущего обрезанного значения.

Важно: условие строгое `>`/`<`, поэтому при `|delta| == 11` флаг `hit` не ставится.

### 2.3. Буфер и среднее

В `Sensor::ApplyTemperatureClampedBufferAndAverage()`:
- обрезанное значение пишется в кольцо `T_Clamped[timeSec % TQ][sens]`;
- затем `SetAverageTemperature()` считает среднее по `T_Clamped`:
  - пока буфер не заполнен (`TimeFromStart < TQ`) — по доступной длине,
  - после заполнения — по полному окну `TQ`;
- результат кладется в `Param 4` (`T_Average`).

### 2.4. Защёлка аварии шумящего датчика

В `EvaluateClampAlarmForSensor()`:
- для температурных/TH-каналов (`TypeOfSensor == 1` или `2`) суммируются `T_ClampHit` по окну `TQ`;
- если `sumHits > TQ/2`, устанавливается бит `Model::Sensor_AlarmFlags`.

Текущее поведение регистра: защёлка только устанавливается (`|=`), автосброса в этом месте нет.

---

## 3) Что именно использует алгоритм управления

В `DefrostControl.cpp`:
- `FilteredSensorT_Deci()` берет `Sensor::GetData(..., Param=4)`;
- воздушный контур (`ComputeDefrostAirControlInputs`) использует только отфильтрованные T;
- датчик исключается из использования, если:
  - канал не активен, или
  - установлен его бит в `Model::Sensor_AlarmFlags` (`SensorExcludedByClampAlarm()`).

Для датчиков продукта (рыба) также применяется исключение по `Sensor_AlarmFlags` при расчете фаз/ограничений.

---

## 4) Что отправляется на сервер

### 4.1. Телеметрия (SEND_STATE)

`Data_CurrentTelemetry()` формирует `MSGQUEUE_OBJ_t`:
- `T[i] = Sensor::GetData(..., Param=2)` -> сырые температуры,
- `H[i] = Sensor::GetData(..., Param=3)`.

Ответ на `REQ_CMD_SEND_STATE` отдается в `CommandReceiver_HandleRequest()`.

### 4.2. Лог алгоритма

После `TELEMETRY_DATA_OK` серверу отправляется лог `ControlLogPayload_t`:
- формируется из `DefrostControl_GetControlLogPayload()`,
- `T_filt_C[]` заполняется через `Sensor::GetData(..., Param=4)` (отфильтрованные температуры).

---

## 5) Формирование общего аварийного флага

Общий аварийный признак формируется по ИЛИ:
- `Gate_Alarm`
- `Device_AlarmFlags`
- `Sensor_AlarmFlags`

Это делается как в `Data.cpp`, так и в `DefrostControl.cpp` (`UpdateDeviceAlarmState()`), и далее влияет на индикацию `_Alr`.

---

## 6) Вывод

По текущей реализации требования по шумозащите температуры, применению фильтрованных значений в управлении, раздельной передаче сырых/фильтрованных значений и участию аварий датчиков в общем флаге аварии выполняются.
