# Защита UART4 мьютексом

## 📋 Проблема

UART4 используется **тремя** конкурирующими процессами:

1. **Отправка телеметрии** на сервер (`WriteToServer`)
2. **Приём команд** от сервера (`CommandReceiver`)
3. **Программирование датчиков** (`ProgrammingSensor`)

Без синхронизации возникает **конфликт доступа** к ресурсу UART4, что приводит к:
- Потере данных
- Искажению пакетов
- Некорректной работе RS-485 (Half Duplex)

## ✅ Решение

Добавлен **мьютекс `UART4_MutexHandle`** для защиты доступа к UART4.

### 🔧 Изменения в коде

#### 1. Объявление мьютекса (`Core/Src/main.c`)

```cpp
/* Definitions for UART4_Mutex */
osMutexId_t UART4_MutexHandle;
const osMutexAttr_t UART4_Mutex_attributes = {
  .name = "UART4_Mutex"
};
```

#### 2. Создание мьютекса (`Core/Src/main.c`)

```cpp
/* creation of UART4_Mutex */
UART4_MutexHandle = osMutexNew(&UART4_Mutex_attributes);
```

#### 3. Использование в `WriteToServer` (`ModBus.cpp`)

```cpp
void WriteToServer(uint8_t* Data, int length)
{
    // ЗАХВАТ МЬЮТЕКСА
    osStatus_t mutexStatus = osMutexAcquire(UART4_MutexHandle, osWaitForever);
    if (mutexStatus != osOK)
    {
        return;  // Критическая ошибка
    }
    
    // Работа с UART4 (отправка телеметрии)
    Master_SendTelemetry(&MB, length);
    CommandReceiver_RestartReception();
    
    // ОСВОБОЖДЕНИЕ МЬЮТЕКСА
    osMutexRelease(UART4_MutexHandle);
}
```

#### 4. Использование в `ProgrammingSensor` (`ModBus.cpp`)

```cpp
while (Model::Type_of_sensor == TypeOfSens)
{
    // ЗАХВАТ МЬЮТЕКСА перед работой с датчиком
    osStatus_t mutexStatus = osMutexAcquire(UART4_MutexHandle, osWaitForever);
    if (mutexStatus != osOK)
    {
        osDelay(10);
        continue;  // Повторяем попытку
    }
    
    // Работа с UART4 (программирование датчика)
    result = ScanSensor(&PR);
    result = WriteToSensor(&PR);
    
    // ОСВОБОЖДЕНИЕ МЬЮТЕКСА
    osMutexRelease(UART4_MutexHandle);
}
```

## 🔒 Как работает мьютекс

### Принцип работы

```
┌─────────────────────────────────────────────────────────────┐
│                    UART4 (общий ресурс)                      │
└────────────────────────┬────────────────────────────────────┘
                         │
                    UART4_Mutex
                         │
        ┌────────────────┼────────────────┐
        │                │                │
┌───────▼────────┐  ┌───▼───────┐  ┌────▼──────────┐
│  WriteToServer │  │CommandRcv │  │Programming    │
│  (телеметрия)  │  │ (приём)   │  │Sensor         │
└────────────────┘  └───────────┘  └───────────────┘
```

### Последовательность доступа

```
Поток 1: WriteToServer
  ├─► osMutexAcquire(UART4_Mutex) ✅ Получил
  ├─► Работа с UART4
  ├─► osMutexRelease(UART4_Mutex)
  └─► Возврат

Поток 2: ProgrammingSensor
  ├─► osMutexAcquire(UART4_Mutex) ⏳ Ждёт освобождения
  │   (блокируется пока Поток 1 держит мьютекс)
  ├─► ✅ Поток 1 освободил → Получил мьютекс
  ├─► Работа с UART4
  ├─► osMutexRelease(UART4_Mutex)
  └─► Возврат
```

## 📊 Сценарии

### Сценарий 1: Телеметрия во время программирования

```
Время →
═══════════════════════════════════════════════════════════════

0 мс:   ProgrammingSensor захватывает UART4_Mutex ✅
        ↓
        Работа с UART4 (ScanSensor, WriteToSensor)
        ↓
100 мс: WriteToServer пытается захватить UART4_Mutex ⏳
        ↓ (БЛОКИРУЕТСЯ, ждёт)
        │
200 мс: ProgrammingSensor освобождает UART4_Mutex ✅
        ↓
        WriteToServer получает UART4_Mutex ✅
        ↓
        Отправка телеметрии
        ↓
250 мс: WriteToServer освобождает UART4_Mutex ✅

РЕЗУЛЬТАТ: Операции выполнились ПОСЛЕДОВАТЕЛЬНО, без конфликтов ✅
```

### Сценарий 2: Программирование во время приёма команды

```
Время →
═══════════════════════════════════════════════════════════════

0 мс:   CommandReceiver слушает линию (НЕ держит мьютекс)
        ↓
50 мс:  ProgrammingSensor захватывает UART4_Mutex ✅
        ↓
        CheckAndWaitForActiveReception() видит приём →
        → ждёт завершения приёма (оsSemaphoreAcquire)
        ↓
100 мс: CommandReceiver получил данные, освободил семафор ✅
        ↓
        ProgrammingSensor продолжает работу с UART4
        ↓
200 мс: ProgrammingSensor освобождает UART4_Mutex ✅

РЕЗУЛЬТАТ: Приём команды завершён, затем программирование ✅
```

## 🛡️ Преимущества решения

### 1. **Взаимное исключение (Mutual Exclusion)**
Только один поток может работать с UART4 в любой момент времени.

### 2. **Отсутствие гонок (No Race Conditions)**
Операции TX/RX не конкурируют друг с другом.

### 3. **Детерминированность**
Предсказуемый порядок доступа к UART4.

### 4. **Защита RS-485 Half Duplex**
Нет одновременных TX/RX на одной линии.

### 5. **Совместимость с существующей архитектурой**
Минимальные изменения, не ломает текущую логику.

## ⚠️ Важные замечания

### 1. **Приоритет потоков**
Если `ProgrammingSensor` имеет более низкий приоритет, чем `WriteToServer`, телеметрия будет отправляться чаще.

### 2. **Таймаут захвата мьютекса**
Используется `osWaitForever` - поток будет ждать бесконечно. Можно изменить на фиксированный таймаут:
```cpp
osMutexAcquire(UART4_MutexHandle, 1000);  // Ждать не более 1 секунды
```

### 3. **Deadlock Prevention**
Убедитесь что:
- Мьютекс **всегда освобождается** после захвата
- Нет вложенных захватов одного и того же мьютекса
- Порядок захвата нескольких мьютексов одинаков во всех потоках

### 4. **CommandReceiver НЕ захватывает мьютекс**
`CommandReceiver` работает **асинхронно** и не нуждается в мьютексе:
- Приём данных идёт через DMA в фоновом режиме
- Обработка происходит после получения данных
- Конфликт исключён через функцию `CheckAndWaitForActiveReception`

## 🔍 Отладка

### Проверка захвата мьютекса

Добавьте отладочный вывод:

```cpp
osStatus_t mutexStatus = osMutexAcquire(UART4_MutexHandle, osWaitForever);
if (mutexStatus != osOK)
{
    // Лог ошибки
    printf("ERROR: Failed to acquire UART4 mutex!\n");
    return;
}

// Лог успешного захвата
printf("UART4 mutex acquired by %s\n", osThreadGetName(osThreadGetId()));

// ... работа с UART4 ...

osMutexRelease(UART4_MutexHandle);
printf("UART4 mutex released by %s\n", osThreadGetName(osThreadGetId()));
```

### Мониторинг блокировок

Можно отслеживать сколько времени поток ждёт мьютекс:

```cpp
uint32_t startTime = osKernelGetTickCount();
osStatus_t mutexStatus = osMutexAcquire(UART4_MutexHandle, 5000);
uint32_t waitTime = osKernelGetTickCount() - startTime;

if (waitTime > 1000)
{
    printf("WARNING: Waited %lu ms for UART4 mutex!\n", waitTime);
}
```

## 📝 Changelog

**10.11.2025**
- Добавлен мьютекс `UART4_Mutex` для защиты доступа к UART4
- Интегрирован в функции `WriteToServer` и `ProgrammingSensor`
- Решён конфликт между телеметрией и программированием датчиков

---

**Дата:** 10 ноября 2025  
**Версия:** 1.0  
**Автор:** AI Assistant (Claude)

