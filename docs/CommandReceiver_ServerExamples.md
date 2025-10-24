# Примеры кода для серверной стороны

**Модуль:** CommandReceiver Integration Examples  
**Назначение:** Примеры реализации клиента для взаимодействия с модулем CommandReceiver  
**Дата:** 24 октября 2025  

---

## Содержание

1. [Python примеры](#python-примеры)
2. [C# примеры](#c-примеры)
3. [JavaScript/Node.js примеры](#javascriptnodejs-примеры)
4. [Утилиты для тестирования](#утилиты-для-тестирования)

---

## Python примеры

### Базовый класс для работы с протоколом

```python
import serial
import struct
import time

class DefrostCommandClient:
    """
    Клиент для работы с модулем CommandReceiver
    """
    
    # Типы команд
    CMD_TYPE_PROG_CONTROL = 0x01
    CMD_TYPE_CONFIGURATION = 0x02
    CMD_TYPE_REQUEST = 0x03
    CMD_TYPE_DEVICE_CONTROL = 0x04
    CMD_TYPE_RESPONSE = 0x80
    
    # Коды команд управления программой
    PROG_CTRL_CMD_START = 0x01
    PROG_CTRL_CMD_STOP = 0x02
    PROG_CTRL_CMD_PAUSE = 0x03
    PROG_CTRL_CMD_RESUME = 0x04
    PROG_CTRL_CMD_RESET = 0x05
    
    # Коды команд управления устройствами
    DEV_CTRL_CMD_RELAY_ON = 0x01
    DEV_CTRL_CMD_RELAY_OFF = 0x02
    DEV_CTRL_CMD_RELAY_SET = 0x03
    DEV_CTRL_CMD_HEATER_ON = 0x04
    DEV_CTRL_CMD_HEATER_OFF = 0x05
    DEV_CTRL_CMD_FAN_ON = 0x06
    DEV_CTRL_CMD_FAN_OFF = 0x07
    
    # Коды команд конфигурации
    CFG_CMD_SET_TEMPERATURE = 0x01
    CFG_CMD_SET_INTERVAL = 0x02
    CFG_CMD_SET_MODE = 0x03
    
    # Коды команд запроса
    REQ_CMD_GET_STATUS = 0x01
    REQ_CMD_GET_VERSION = 0x02
    REQ_CMD_GET_CONFIG = 0x03
    
    # Статусы ответа
    CMD_STATUS_OK = 0x00
    CMD_STATUS_CRC_ERROR = 0x01
    CMD_STATUS_INVALID_TYPE = 0x02
    CMD_STATUS_INVALID_CODE = 0x03
    CMD_STATUS_INVALID_LENGTH = 0x04
    CMD_STATUS_EXECUTION_ERROR = 0x05
    CMD_STATUS_TIMEOUT = 0x06
    
    # Таблица CRC16 (ModBus)
    CRC16_TABLE = [
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
    ]
    
    def __init__(self, port, baudrate=19200, timeout=5):
        """
        Инициализация клиента
        
        Args:
            port: COM-порт (например, 'COM3' или '/dev/ttyUSB0')
            baudrate: скорость обмена
            timeout: таймаут чтения в секундах
        """
        self.serial = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=timeout
        )
        
    def calculate_crc(self, data):
        """Вычисление CRC16 ModBus"""
        crc = 0xFFFF
        for byte in data:
            crc = (crc >> 8) ^ self.CRC16_TABLE[(byte ^ crc) & 0xFF]
        return crc
    
    def send_command(self, cmd_type, cmd_code, data=b''):
        """
        Отправка команды контроллеру
        
        Args:
            cmd_type: тип команды
            cmd_code: код команды
            data: данные команды (bytes)
        
        Returns:
            dict с полями response или None в случае ошибки
        """
        # Формирование пакета
        data_len = len(data)
        packet = struct.pack('BBB', cmd_type, cmd_code, data_len)
        packet += data
        
        # Вычисление и добавление CRC
        crc = self.calculate_crc(packet)
        packet += struct.pack('<H', crc)  # Little Endian
        
        # Отправка
        self.serial.write(packet)
        time.sleep(0.01)  # Небольшая задержка
        
        # Ожидание ответа
        response = self.receive_response()
        return response
    
    def receive_response(self):
        """
        Прием ответа от контроллера
        
        Returns:
            dict с полями: type, code, status, data или None
        """
        # Читаем минимум 6 байт (заголовок + CRC)
        header = self.serial.read(4)
        if len(header) < 4:
            return None
        
        resp_type, resp_code, status, data_len = struct.unpack('BBBB', header)
        
        # Читаем данные и CRC
        remaining = self.serial.read(data_len + 2)
        if len(remaining) < data_len + 2:
            return None
        
        data = remaining[:data_len]
        crc_received = struct.unpack('<H', remaining[data_len:data_len+2])[0]
        
        # Проверка CRC
        packet = header + data
        crc_calculated = self.calculate_crc(packet)
        
        if crc_received != crc_calculated:
            return {'error': 'CRC mismatch'}
        
        return {
            'type': resp_type,
            'code': resp_code,
            'status': status,
            'data': data
        }
    
    def start_program(self):
        """Запуск программы"""
        return self.send_command(self.CMD_TYPE_PROG_CONTROL, self.PROG_CTRL_CMD_START)
    
    def stop_program(self):
        """Остановка программы"""
        return self.send_command(self.CMD_TYPE_PROG_CONTROL, self.PROG_CTRL_CMD_STOP)
    
    def relay_on(self, relay_num):
        """Включение реле"""
        data = struct.pack('B', relay_num)
        return self.send_command(self.CMD_TYPE_DEVICE_CONTROL, self.DEV_CTRL_CMD_RELAY_ON, data)
    
    def relay_off(self, relay_num):
        """Выключение реле"""
        data = struct.pack('B', relay_num)
        return self.send_command(self.CMD_TYPE_DEVICE_CONTROL, self.DEV_CTRL_CMD_RELAY_OFF, data)
    
    def set_all_relays(self, mask):
        """Установка состояния всех реле (битовая маска)"""
        data = struct.pack('<H', mask)
        return self.send_command(self.CMD_TYPE_DEVICE_CONTROL, self.DEV_CTRL_CMD_RELAY_SET, data)
    
    def heater_on(self):
        """Включение всех нагревателей"""
        return self.send_command(self.CMD_TYPE_DEVICE_CONTROL, self.DEV_CTRL_CMD_HEATER_ON)
    
    def heater_off(self):
        """Выключение всех нагревателей"""
        return self.send_command(self.CMD_TYPE_DEVICE_CONTROL, self.DEV_CTRL_CMD_HEATER_OFF)
    
    def fan_on(self):
        """Включение всех вентиляторов"""
        return self.send_command(self.CMD_TYPE_DEVICE_CONTROL, self.DEV_CTRL_CMD_FAN_ON)
    
    def fan_off(self):
        """Выключение всех вентиляторов"""
        return self.send_command(self.CMD_TYPE_DEVICE_CONTROL, self.DEV_CTRL_CMD_FAN_OFF)
    
    def set_temperature(self, temp):
        """Установка целевой температуры"""
        data = struct.pack('<f', temp)
        return self.send_command(self.CMD_TYPE_CONFIGURATION, self.CFG_CMD_SET_TEMPERATURE, data)
    
    def set_interval(self, interval):
        """Установка интервала измерений (секунды)"""
        data = struct.pack('<H', interval)
        return self.send_command(self.CMD_TYPE_CONFIGURATION, self.CFG_CMD_SET_INTERVAL, data)
    
    def set_mode(self, mode):
        """Установка режима работы (0=авто, 1=ручной)"""
        data = struct.pack('B', mode)
        return self.send_command(self.CMD_TYPE_CONFIGURATION, self.CFG_CMD_SET_MODE, data)
    
    def get_status(self):
        """Запрос текущего статуса"""
        response = self.send_command(self.CMD_TYPE_REQUEST, self.REQ_CMD_GET_STATUS)
        if response and response.get('status') == self.CMD_STATUS_OK:
            status_value = struct.unpack('<H', response['data'])[0]
            return status_value
        return None
    
    def get_version(self):
        """Запрос версии прошивки"""
        response = self.send_command(self.CMD_TYPE_REQUEST, self.REQ_CMD_GET_VERSION)
        if response and response.get('status') == self.CMD_STATUS_OK:
            return response['data'].decode('ascii')
        return None
    
    def get_config(self):
        """Запрос текущей конфигурации"""
        response = self.send_command(self.CMD_TYPE_REQUEST, self.REQ_CMD_GET_CONFIG)
        if response and response.get('status') == self.CMD_STATUS_OK:
            mode = struct.unpack('B', response['data'])[0]
            return mode
        return None
    
    def close(self):
        """Закрытие соединения"""
        self.serial.close()


# Пример использования
if __name__ == '__main__':
    # Подключение к контроллеру
    client = DefrostCommandClient('COM3', baudrate=19200)
    
    try:
        # Запрос версии
        version = client.get_version()
        print(f"Версия прошивки: {version}")
        
        # Запуск программы
        response = client.start_program()
        if response and response['status'] == DefrostCommandClient.CMD_STATUS_OK:
            print("Программа запущена успешно")
        
        # Включение реле №5
        response = client.relay_on(5)
        print(f"Включение реле 5: {response}")
        
        # Запрос статуса
        status = client.get_status()
        print(f"Текущий статус: 0x{status:04X}")
        
        # Установка температуры
        response = client.set_temperature(25.5)
        print(f"Установка температуры: {response}")
        
        # Включение всех нагревателей
        response = client.heater_on()
        print(f"Включение нагревателей: {response}")
        
    finally:
        client.close()
```

---

## C# примеры

### Класс для работы с протоколом

```csharp
using System;
using System.IO.Ports;
using System.Threading;

namespace DefrostController
{
    public class DefrostCommandClient : IDisposable
    {
        // Типы команд
        public const byte CMD_TYPE_PROG_CONTROL = 0x01;
        public const byte CMD_TYPE_CONFIGURATION = 0x02;
        public const byte CMD_TYPE_REQUEST = 0x03;
        public const byte CMD_TYPE_DEVICE_CONTROL = 0x04;
        public const byte CMD_TYPE_RESPONSE = 0x80;
        
        // Коды команд управления программой
        public const byte PROG_CTRL_CMD_START = 0x01;
        public const byte PROG_CTRL_CMD_STOP = 0x02;
        public const byte PROG_CTRL_CMD_PAUSE = 0x03;
        public const byte PROG_CTRL_CMD_RESUME = 0x04;
        public const byte PROG_CTRL_CMD_RESET = 0x05;
        
        // Коды команд управления устройствами
        public const byte DEV_CTRL_CMD_RELAY_ON = 0x01;
        public const byte DEV_CTRL_CMD_RELAY_OFF = 0x02;
        public const byte DEV_CTRL_CMD_RELAY_SET = 0x03;
        public const byte DEV_CTRL_CMD_HEATER_ON = 0x04;
        public const byte DEV_CTRL_CMD_HEATER_OFF = 0x05;
        public const byte DEV_CTRL_CMD_FAN_ON = 0x06;
        public const byte DEV_CTRL_CMD_FAN_OFF = 0x07;
        
        // Статусы ответа
        public const byte CMD_STATUS_OK = 0x00;
        public const byte CMD_STATUS_CRC_ERROR = 0x01;
        
        private SerialPort _serialPort;
        private static readonly ushort[] CRC16_TABLE = new ushort[256]
        {
            0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
            0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
            // ... (полная таблица как в Python примере)
        };
        
        public DefrostCommandClient(string portName, int baudRate = 19200)
        {
            _serialPort = new SerialPort(portName, baudRate, Parity.None, 8, StopBits.One);
            _serialPort.ReadTimeout = 5000;
            _serialPort.WriteTimeout = 1000;
            _serialPort.Open();
        }
        
        private ushort CalculateCRC(byte[] data, int length)
        {
            ushort crc = 0xFFFF;
            for (int i = 0; i < length; i++)
            {
                crc = (ushort)((crc >> 8) ^ CRC16_TABLE[(data[i] ^ crc) & 0xFF]);
            }
            return crc;
        }
        
        public CommandResponse SendCommand(byte cmdType, byte cmdCode, byte[] data = null)
        {
            if (data == null) data = new byte[0];
            
            // Формирование пакета
            int packetSize = 3 + data.Length + 2;
            byte[] packet = new byte[packetSize];
            
            packet[0] = cmdType;
            packet[1] = cmdCode;
            packet[2] = (byte)data.Length;
            Array.Copy(data, 0, packet, 3, data.Length);
            
            // Вычисление CRC
            ushort crc = CalculateCRC(packet, 3 + data.Length);
            packet[3 + data.Length] = (byte)(crc & 0xFF);
            packet[3 + data.Length + 1] = (byte)((crc >> 8) & 0xFF);
            
            // Отправка
            _serialPort.Write(packet, 0, packetSize);
            Thread.Sleep(10);
            
            // Прием ответа
            return ReceiveResponse();
        }
        
        private CommandResponse ReceiveResponse()
        {
            try
            {
                // Чтение заголовка
                byte[] header = new byte[4];
                int bytesRead = _serialPort.Read(header, 0, 4);
                if (bytesRead < 4) return null;
                
                byte respType = header[0];
                byte respCode = header[1];
                byte status = header[2];
                byte dataLen = header[3];
                
                // Чтение данных и CRC
                byte[] remaining = new byte[dataLen + 2];
                bytesRead = _serialPort.Read(remaining, 0, dataLen + 2);
                if (bytesRead < dataLen + 2) return null;
                
                byte[] data = new byte[dataLen];
                Array.Copy(remaining, 0, data, 0, dataLen);
                
                ushort crcReceived = (ushort)(remaining[dataLen] | (remaining[dataLen + 1] << 8));
                
                // Проверка CRC
                byte[] fullPacket = new byte[4 + dataLen];
                Array.Copy(header, fullPacket, 4);
                Array.Copy(data, 0, fullPacket, 4, dataLen);
                
                ushort crcCalculated = CalculateCRC(fullPacket, fullPacket.Length);
                
                if (crcReceived != crcCalculated)
                {
                    return new CommandResponse { Status = 0xFF, Error = "CRC mismatch" };
                }
                
                return new CommandResponse
                {
                    Type = respType,
                    Code = respCode,
                    Status = status,
                    Data = data
                };
            }
            catch (TimeoutException)
            {
                return null;
            }
        }
        
        // Высокоуровневые методы
        public CommandResponse StartProgram()
        {
            return SendCommand(CMD_TYPE_PROG_CONTROL, PROG_CTRL_CMD_START);
        }
        
        public CommandResponse StopProgram()
        {
            return SendCommand(CMD_TYPE_PROG_CONTROL, PROG_CTRL_CMD_STOP);
        }
        
        public CommandResponse RelayOn(byte relayNum)
        {
            return SendCommand(CMD_TYPE_DEVICE_CONTROL, DEV_CTRL_CMD_RELAY_ON, new byte[] { relayNum });
        }
        
        public CommandResponse RelayOff(byte relayNum)
        {
            return SendCommand(CMD_TYPE_DEVICE_CONTROL, DEV_CTRL_CMD_RELAY_OFF, new byte[] { relayNum });
        }
        
        public CommandResponse SetAllRelays(ushort mask)
        {
            byte[] data = BitConverter.GetBytes(mask);
            return SendCommand(CMD_TYPE_DEVICE_CONTROL, DEV_CTRL_CMD_RELAY_SET, data);
        }
        
        public ushort? GetStatus()
        {
            var response = SendCommand(CMD_TYPE_REQUEST, 0x01);
            if (response?.Status == CMD_STATUS_OK && response.Data.Length >= 2)
            {
                return BitConverter.ToUInt16(response.Data, 0);
            }
            return null;
        }
        
        public string GetVersion()
        {
            var response = SendCommand(CMD_TYPE_REQUEST, 0x02);
            if (response?.Status == CMD_STATUS_OK)
            {
                return System.Text.Encoding.ASCII.GetString(response.Data);
            }
            return null;
        }
        
        public void Dispose()
        {
            _serialPort?.Close();
            _serialPort?.Dispose();
        }
    }
    
    public class CommandResponse
    {
        public byte Type { get; set; }
        public byte Code { get; set; }
        public byte Status { get; set; }
        public byte[] Data { get; set; }
        public string Error { get; set; }
    }
    
    // Пример использования
    class Program
    {
        static void Main(string[] args)
        {
            using (var client = new DefrostCommandClient("COM3", 19200))
            {
                // Запрос версии
                string version = client.GetVersion();
                Console.WriteLine($"Версия: {version}");
                
                // Запуск программы
                var response = client.StartProgram();
                Console.WriteLine($"Запуск: статус {response.Status}");
                
                // Включение реле 5
                response = client.RelayOn(5);
                Console.WriteLine($"Реле 5: статус {response.Status}");
                
                // Запрос статуса
                ushort? status = client.GetStatus();
                Console.WriteLine($"Статус: 0x{status:X4}");
            }
        }
    }
}
```

---

## JavaScript/Node.js примеры

### Модуль для работы с протоколом

```javascript
const SerialPort = require('serialport');

class DefrostCommandClient {
    // Типы команд
    static CMD_TYPE_PROG_CONTROL = 0x01;
    static CMD_TYPE_CONFIGURATION = 0x02;
    static CMD_TYPE_REQUEST = 0x03;
    static CMD_TYPE_DEVICE_CONTROL = 0x04;
    
    // Коды команд
    static PROG_CTRL_CMD_START = 0x01;
    static PROG_CTRL_CMD_STOP = 0x02;
    static DEV_CTRL_CMD_RELAY_ON = 0x01;
    static DEV_CTRL_CMD_RELAY_OFF = 0x02;
    
    // Таблица CRC16
    static CRC16_TABLE = [
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        // ... (полная таблица)
    ];
    
    constructor(portPath, baudRate = 19200) {
        this.port = new SerialPort(portPath, {
            baudRate: baudRate,
            dataBits: 8,
            parity: 'none',
            stopBits: 1
        });
        
        this.responseBuffer = Buffer.alloc(0);
        this.responseCallback = null;
        
        this.port.on('data', (data) => {
            this.responseBuffer = Buffer.concat([this.responseBuffer, data]);
            this.processResponse();
        });
    }
    
    calculateCRC(data) {
        let crc = 0xFFFF;
        for (let i = 0; i < data.length; i++) {
            crc = (crc >> 8) ^ DefrostCommandClient.CRC16_TABLE[(data[i] ^ crc) & 0xFF];
        }
        return crc;
    }
    
    sendCommand(cmdType, cmdCode, data = Buffer.alloc(0)) {
        return new Promise((resolve, reject) => {
            // Формирование пакета
            const dataLen = data.length;
            const packet = Buffer.allocUnsafe(3 + dataLen + 2);
            
            packet.writeUInt8(cmdType, 0);
            packet.writeUInt8(cmdCode, 1);
            packet.writeUInt8(dataLen, 2);
            
            if (dataLen > 0) {
                data.copy(packet, 3);
            }
            
            // Вычисление CRC
            const crc = this.calculateCRC(packet.slice(0, 3 + dataLen));
            packet.writeUInt16LE(crc, 3 + dataLen);
            
            // Установка callback для ответа
            this.responseCallback = resolve;
            
            // Отправка
            this.port.write(packet, (err) => {
                if (err) {
                    reject(err);
                }
            });
            
            // Таймаут
            setTimeout(() => {
                if (this.responseCallback === resolve) {
                    this.responseCallback = null;
                    reject(new Error('Response timeout'));
                }
            }, 5000);
        });
    }
    
    processResponse() {
        if (this.responseBuffer.length < 6) return;
        
        const respType = this.responseBuffer.readUInt8(0);
        const respCode = this.responseBuffer.readUInt8(1);
        const status = this.responseBuffer.readUInt8(2);
        const dataLen = this.responseBuffer.readUInt8(3);
        
        const totalLen = 4 + dataLen + 2;
        if (this.responseBuffer.length < totalLen) return;
        
        const data = this.responseBuffer.slice(4, 4 + dataLen);
        const crcReceived = this.responseBuffer.readUInt16LE(4 + dataLen);
        
        // Проверка CRC
        const crcCalculated = this.calculateCRC(this.responseBuffer.slice(0, 4 + dataLen));
        
        const response = {
            type: respType,
            code: respCode,
            status: status,
            data: data,
            crcValid: crcReceived === crcCalculated
        };
        
        // Очистка буфера
        this.responseBuffer = this.responseBuffer.slice(totalLen);
        
        // Вызов callback
        if (this.responseCallback) {
            const cb = this.responseCallback;
            this.responseCallback = null;
            cb(response);
        }
    }
    
    // Высокоуровневые методы
    async startProgram() {
        return await this.sendCommand(
            DefrostCommandClient.CMD_TYPE_PROG_CONTROL,
            DefrostCommandClient.PROG_CTRL_CMD_START
        );
    }
    
    async stopProgram() {
        return await this.sendCommand(
            DefrostCommandClient.CMD_TYPE_PROG_CONTROL,
            DefrostCommandClient.PROG_CTRL_CMD_STOP
        );
    }
    
    async relayOn(relayNum) {
        const data = Buffer.allocUnsafe(1);
        data.writeUInt8(relayNum, 0);
        return await this.sendCommand(
            DefrostCommandClient.CMD_TYPE_DEVICE_CONTROL,
            DefrostCommandClient.DEV_CTRL_CMD_RELAY_ON,
            data
        );
    }
    
    async relayOff(relayNum) {
        const data = Buffer.allocUnsafe(1);
        data.writeUInt8(relayNum, 0);
        return await this.sendCommand(
            DefrostCommandClient.CMD_TYPE_DEVICE_CONTROL,
            DefrostCommandClient.DEV_CTRL_CMD_RELAY_OFF,
            data
        );
    }
    
    async getStatus() {
        const response = await this.sendCommand(
            DefrostCommandClient.CMD_TYPE_REQUEST,
            0x01
        );
        if (response.status === 0x00 && response.data.length >= 2) {
            return response.data.readUInt16LE(0);
        }
        return null;
    }
    
    close() {
        this.port.close();
    }
}

// Пример использования
async function main() {
    const client = new DefrostCommandClient('/dev/ttyUSB0', 19200);
    
    try {
        // Запуск программы
        const response = await client.startProgram();
        console.log('Запуск программы:', response.status === 0x00 ? 'OK' : 'ERROR');
        
        // Включение реле 5
        await client.relayOn(5);
        console.log('Реле 5 включено');
        
        // Запрос статуса
        const status = await client.getStatus();
        console.log(`Статус: 0x${status.toString(16).padStart(4, '0')}`);
        
    } catch (error) {
        console.error('Ошибка:', error);
    } finally {
        client.close();
    }
}

module.exports = DefrostCommandClient;
```

---

## Утилиты для тестирования

### Python скрипт для интерактивного тестирования

```python
import sys
from defrost_client import DefrostCommandClient

def print_menu():
    print("\n=== Тестирование DefrostController ===")
    print("1. Запустить программу")
    print("2. Остановить программу")
    print("3. Включить реле")
    print("4. Выключить реле")
    print("5. Включить нагреватели")
    print("6. Выключить нагреватели")
    print("7. Запросить статус")
    print("8. Запросить версию")
    print("0. Выход")
    print("=====================================")

def main():
    if len(sys.argv) < 2:
        print("Использование: python test_client.py <COM-порт>")
        sys.exit(1)
    
    port = sys.argv[1]
    client = DefrostCommandClient(port)
    
    try:
        while True:
            print_menu()
            choice = input("Выберите действие: ")
            
            if choice == '1':
                response = client.start_program()
                print(f"Результат: {response}")
                
            elif choice == '2':
                response = client.stop_program()
                print(f"Результат: {response}")
                
            elif choice == '3':
                relay_num = int(input("Номер реле (0-15): "))
                response = client.relay_on(relay_num)
                print(f"Результат: {response}")
                
            elif choice == '4':
                relay_num = int(input("Номер реле (0-15): "))
                response = client.relay_off(relay_num)
                print(f"Результат: {response}")
                
            elif choice == '5':
                response = client.heater_on()
                print(f"Результат: {response}")
                
            elif choice == '6':
                response = client.heater_off()
                print(f"Результат: {response}")
                
            elif choice == '7':
                status = client.get_status()
                print(f"Статус: 0x{status:04X}")
                print(f"Бинарно: {bin(status)}")
                
            elif choice == '8':
                version = client.get_version()
                print(f"Версия: {version}")
                
            elif choice == '0':
                break
                
            else:
                print("Неверный выбор")
    
    finally:
        client.close()
        print("Соединение закрыто")

if __name__ == '__main__':
    main()
```

---

## Заметки по реализации

### Важные моменты:

1. **Порядок байтов CRC:** Little Endian (младший байт первый)
2. **Таймауты:** Рекомендуется 5 секунд для приема ответа
3. **Задержки:** Небольшая задержка (10 мс) после отправки перед чтением
4. **Проверка CRC:** Обязательна для обеспечения целостности данных

### Отладка:

Для отладки протокола удобно логировать все отправленные и полученные байты в hex формате:

```python
def log_packet(direction, data):
    hex_str = ' '.join([f'{b:02X}' for b in data])
    print(f"{direction}: {hex_str}")

# Использование
log_packet("TX", packet)
log_packet("RX", response_data)
```

---

*Конец документа*

