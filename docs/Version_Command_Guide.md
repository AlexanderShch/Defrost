# 📡 Команда запроса версии прошивки

**Команда**: REQ_CMD_GET_VERSION (03 02)  
**Версия прошивки**: 1.1.0  
**Дата**: 12 ноября 2025

---

## ✅ Текущее состояние

Команда **полностью реализована и работает**! ✨

### 📋 Реализация

**Файл**: `STM32CubeIDE/ProjectCode/CommandReceiver.cpp`

```cpp
case REQ_CMD_GET_VERSION:
{
    // Отправляем версию прошивки из version.h
    // Формат ответа: MAJOR.MINOR.PATCH (строка)
    const char *version = FW_VERSION_STRING;  // "1.1.0"
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
```

---

## 📨 Протокол команды

### Запрос от сервера

```
┌──────┬──────┬────────┬──────────┐
│ Type │ Code │ DataLen│   CRC    │
├──────┼──────┼────────┼──────────┤
│ 0x03 │ 0x02 │  0x00  │ CRC16 LH │
└──────┴──────┴────────┴──────────┘
```

**Байты:**
- `[0]` = `0x03` - CommandType: REQUEST
- `[1]` = `0x02` - CommandCode: GET_VERSION
- `[2]` = `0x00` - DataLength: 0 (нет данных в запросе)
- `[3-4]` = CRC16 (младший, старший байт)

**Пример запроса (hex):**
```
03 02 00 [CRC_LO] [CRC_HI]
```

### Ответ от контроллера

```
┌──────┬──────┬────────┬────────┬──────────────────┬──────────┐
│ Type │ Code │ Status │DataLen │      Data        │   CRC    │
├──────┼──────┼────────┼────────┼──────────────────┼──────────┤
│ 0x03 │ 0x02 │  0x00  │  0x05  │ "1.1.0" (ASCII)  │ CRC16 LH │
└──────┴──────┴────────┴────────┴──────────────────┴──────────┘
```

**Байты:**
- `[0]` = `0x03` - CommandType: REQUEST
- `[1]` = `0x02` - CommandCode: GET_VERSION
- `[2]` = `0x00` - Status: OK
- `[3]` = `0x05` - DataLength: 5 байт (длина "1.1.0")
- `[4-8]` = ASCII строка: `0x31 0x2E 0x31 0x2E 0x30` ("1.1.0")
- `[9-10]` = CRC16 (младший, старший байт)

**Пример ответа (hex):**
```
03 02 00 05 31 2E 31 2E 30 [CRC_LO] [CRC_HI]
           │  │  │  │  │
           │  │  │  │  └─ '0' (0x30)
           │  │  │  └──── '.' (0x2E)
           │  │  └─────── '1' (0x31)
           │  └────────── '.' (0x2E)
           └───────────── '1' (0x31)
```

---

## 🐍 Пример использования (Python)

### Базовая отправка команды

```python
import serial
import struct

def calculate_crc16(data):
    """Вычисление CRC16 (ModBus)"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def request_version(ser):
    """Запросить версию прошивки"""
    # Формируем команду
    command = bytearray([
        0x03,  # Type: REQUEST
        0x02,  # Code: GET_VERSION
        0x00   # DataLen: 0
    ])
    
    # Добавляем CRC
    crc = calculate_crc16(command)
    command.append(crc & 0xFF)        # CRC Lo
    command.append((crc >> 8) & 0xFF) # CRC Hi
    
    # Отправляем
    ser.write(command)
    print(f"Отправлено: {command.hex(' ')}")
    
    # Ожидаем ответ
    response = ser.read(100)  # Максимум 100 байт
    
    if len(response) >= 5:
        cmd_type = response[0]
        cmd_code = response[1]
        status = response[2]
        data_len = response[3]
        
        if status == 0x00 and data_len > 0:
            version = response[4:4+data_len].decode('ascii')
            print(f"Версия прошивки: {version}")
            return version
    
    return None

# Использование
ser = serial.Serial('COM3', 19200, timeout=1)
version = request_version(ser)
```

### Полный пример с обработкой ошибок

```python
import serial
import struct
import time

class DefrostController:
    def __init__(self, port, baudrate=19200):
        self.ser = serial.Serial(port, baudrate, timeout=1)
    
    def calculate_crc16(self, data):
        """ModBus CRC16"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc
    
    def send_command(self, cmd_type, cmd_code, data=b''):
        """Отправить команду и получить ответ"""
        # Формируем команду
        command = bytearray([cmd_type, cmd_code, len(data)])
        command.extend(data)
        
        # Добавляем CRC
        crc = self.calculate_crc16(command)
        command.append(crc & 0xFF)
        command.append((crc >> 8) & 0xFF)
        
        # Отправляем
        self.ser.write(command)
        time.sleep(0.1)  # Задержка на обработку
        
        # Получаем ответ
        response = self.ser.read(100)
        return response
    
    def get_version(self):
        """Получить версию прошивки"""
        response = self.send_command(0x03, 0x02)  # REQUEST, GET_VERSION
        
        if len(response) >= 5:
            cmd_type = response[0]
            cmd_code = response[1]
            status = response[2]
            data_len = response[3]
            
            if status == 0x00 and data_len > 0:
                version = response[4:4+data_len].decode('ascii')
                return {
                    'success': True,
                    'version': version,
                    'raw_data': response.hex(' ')
                }
        
        return {'success': False, 'error': 'Invalid response'}
    
    def get_build_info(self):
        """Получить полную информацию о сборке"""
        response = self.send_command(0x03, 0x04)  # REQUEST, GET_BUILD_INFO
        
        if len(response) >= 5:
            status = response[2]
            data_len = response[3]
            
            if status == 0x00 and data_len > 0:
                build_info = response[4:4+data_len].decode('ascii')
                return {
                    'success': True,
                    'build_info': build_info
                }
        
        return {'success': False, 'error': 'Invalid response'}

# Использование
controller = DefrostController('COM3')

# Запрос версии
result = controller.get_version()
if result['success']:
    print(f"Версия: {result['version']}")  # "1.1.0"
else:
    print(f"Ошибка: {result['error']}")

# Запрос полной информации о сборке
build = controller.get_build_info()
if build['success']:
    print(f"Сборка: {build['build_info']}")  # "Defrost Controller v1.1.0 (Nov 12 2025 14:30:00)"
```

---

## 🔧 Тестирование команды

### Вариант 1: Через Serial Monitor

```
1. Подключитесь к COM-порту контроллера (19200 бод)
2. Отправьте (HEX): 03 02 00 [CRC_LO] [CRC_HI]
3. Получите ответ: 03 02 00 05 31 2E 31 2E 30 [CRC]
4. Декодируйте: 31 2E 31 2E 30 → "1.1.0"
```

### Вариант 2: Через Python скрипт

```bash
python test_version.py
```

**test_version.py:**
```python
from defrost_controller import DefrostController

controller = DefrostController('COM3')
result = controller.get_version()
print(f"Результат: {result}")
```

**Ожидаемый вывод:**
```
Результат: {'success': True, 'version': '1.1.0', 'raw_data': '03 02 00 05 31 2e 31 2e 30 [crc]'}
```

---

## 📊 Связанные команды

| Команда | Код | Описание | Ответ |
|---------|-----|----------|-------|
| **GET_VERSION** | 03 02 | Короткая версия | "1.1.0" |
| **GET_BUILD_INFO** | 03 04 | Полная информация | "Defrost Controller v1.1.0 (Nov 12 2025 14:30:00)" |
| **GET_STATUS** | 03 01 | Статус устройства | Регистры состояния |
| **GET_CONFIG** | 03 03 | Конфигурация | Режим работы и параметры |

---

## 🎯 Практические примеры

### Пример 1: Проверка версии при подключении

```python
def connect_and_check_version(port):
    """Подключиться и проверить совместимость версии"""
    controller = DefrostController(port)
    
    result = controller.get_version()
    if result['success']:
        version = result['version']
        major, minor, patch = map(int, version.split('.'))
        
        # Проверка минимальной версии
        if major >= 1 and minor >= 1:
            print(f"✅ Версия {version} поддерживается")
            return True
        else:
            print(f"⚠️ Версия {version} устарела. Требуется обновление.")
            return False
    
    print("❌ Не удалось получить версию")
    return False
```

### Пример 2: Логирование версии в файл

```python
import datetime

def log_device_info(port, log_file='device_log.txt'):
    """Записать информацию об устройстве в лог"""
    controller = DefrostController(port)
    
    version_result = controller.get_version()
    build_result = controller.get_build_info()
    
    with open(log_file, 'a') as f:
        timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        f.write(f"\n[{timestamp}] Подключение к {port}\n")
        
        if version_result['success']:
            f.write(f"Версия: {version_result['version']}\n")
        
        if build_result['success']:
            f.write(f"Сборка: {build_result['build_info']}\n")
```

### Пример 3: Автоматическое обновление при старой версии

```python
def auto_update_if_needed(port, min_version='1.1.0'):
    """Проверить версию и обновить при необходимости"""
    controller = DefrostController(port)
    result = controller.get_version()
    
    if result['success']:
        current = result['version']
        
        if version_compare(current, min_version) < 0:
            print(f"Обнаружена старая версия {current}")
            print(f"Требуется минимум {min_version}")
            print("Запуск обновления...")
            # update_firmware(port)
        else:
            print(f"✅ Версия {current} актуальна")
    
def version_compare(v1, v2):
    """Сравнить две версии: -1 если v1 < v2, 0 если равны, 1 если v1 > v2"""
    v1_parts = list(map(int, v1.split('.')))
    v2_parts = list(map(int, v2.split('.')))
    
    for i in range(3):
        if v1_parts[i] < v2_parts[i]:
            return -1
        elif v1_parts[i] > v2_parts[i]:
            return 1
    return 0
```

---

## 🔍 Отладка

### Проблема: Нет ответа от контроллера

**Возможные причины:**
1. Неправильный COM-порт
2. Неправильная скорость (должна быть 19200)
3. Контроллер занят другой операцией
4. Неправильный CRC

**Решение:**
```python
# Добавьте отладочный вывод
def send_command_debug(self, cmd_type, cmd_code):
    command = bytearray([cmd_type, cmd_code, 0x00])
    crc = self.calculate_crc16(command)
    command.append(crc & 0xFF)
    command.append((crc >> 8) & 0xFF)
    
    print(f"Отправка: {command.hex(' ')}")
    self.ser.write(command)
    
    time.sleep(0.2)
    response = self.ser.read(100)
    print(f"Получено: {response.hex(' ')} ({len(response)} байт)")
    
    return response
```

### Проблема: Неверный CRC в ответе

**Проверка:**
```python
def verify_response_crc(response):
    if len(response) < 5:
        return False
    
    data = response[:-2]  # Все кроме последних 2 байт (CRC)
    received_crc = struct.unpack('<H', response[-2:])[0]
    calculated_crc = calculate_crc16(data)
    
    print(f"Received CRC: 0x{received_crc:04X}")
    print(f"Calculated CRC: 0x{calculated_crc:04X}")
    
    return received_crc == calculated_crc
```

---

## 📚 Дополнительная документация

- [CommandReceiver_Documentation.md](CommandReceiver_Documentation.md) - Полная документация протокола команд
- [CommandReceiver_ServerExamples.md](CommandReceiver_ServerExamples.md) - Больше примеров на Python
- [version.h](../STM32CubeIDE/ProjectCode/version.h) - Определение версии прошивки

---

## ✅ Контрольный список

- [x] ✅ Команда реализована в CommandReceiver.cpp
- [x] ✅ Используется FW_VERSION_STRING из version.h
- [x] ✅ Корректно формируется ответ
- [x] ✅ Проверяется длина версии
- [x] ✅ Отправляется CRC16
- [x] ✅ Работает при версии 1.1.0
- [x] ✅ Документация создана

**Команда полностью готова к использованию!** 🎉

---

**Автор**: AI Assistant (Claude)  
**Дата**: 12 ноября 2025  
**Версия документа**: 1.0

