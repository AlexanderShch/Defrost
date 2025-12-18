# 📡 ИТОГ: Новый формат кадра (AA55 + Type + Len, без маркера конца)

**Дата**: 18 декабря 2025  
**Статус**: ✅ применено

---

## 🎯 Цель

Сделать разбор на сервере устойчивым к появлению `AA55/55AA` внутри данных, добавив длину полезной части сразу после байта типа.

---

## ✅ Формат кадра (ОТ контроллера К серверу)

```
[AA 55][Type][Len][Payload...][CRC16]
```

Где:
- `Len` = длина `Payload` (байты после Len и до CRC)
- `CRC16` считается по `[Type][Len][Payload...]`
- маркер конца `55 AA` **не используется**

---

## 📦 Телеметрия (Type = 0x00)

`Len = 45 (0x2D)`

`Payload` (45 байт): `Time(2) + SQ(1) + SensorType(7) + Active(7) + T(14) + H(14)`

Итого размер на линии: `2 + 1 + 1 + 45 + 2 = 51` байт.

---

## 📦 Ответ на команду (Type = 0x01..0x04)

`Payload`: `[Code][Status][DataLen][Data...]`

`Len = 3 + DataLen`

CRC16 считается по `[Type][Len][Code][Status][DataLen][Data...]`.

---

## 🧩 Реализация в коде

- `STM32CubeIDE/ProjectCode/Data.cpp`
  - телеметрия теперь содержит поле `Len` сразу после `DataType`
  - CRC считается по структуре без последних 2 байт CRC (включая `DataType` и `Len`)
- `STM32CubeIDE/ProjectCode/CommandReceiver.cpp`
  - ответы на команды формируются как `[Type][Len][Code][Status][DataLen][Data...][CRC16]`
- `STM32CubeIDE/ProjectCode/ModBus.cpp`
  - `WriteToServerWithSync*` отправляет `AA55 + Data` (без `55AA`)


