#ifndef EEPROM_HPP_
#define EEPROM_HPP_

#include "main.h"
#include <stdint.h>

namespace EEPROM
{
// Параметры M24C16 (16 Kbit = 2048 байт).
static constexpr uint16_t kSizeBytes = 2048u;
static constexpr uint16_t kPageSizeBytes = 16u;
// Базовый 8-битный адрес HAL (блок 0, A10..A8=000).
// У M24C16 биты A10..A8 памяти входят в device select: 1010 A10 A9 A8 R/W.
static constexpr uint16_t kDefaultDeviceAddress = 0xA0u;

// Инициализация драйвера: сохраняет базовый адрес устройства и проверяет доступность EEPROM на шине.
bool Init(uint16_t deviceAddress = kDefaultDeviceAddress, uint32_t timeoutMs = 100u);

// Чтение блока из EEPROM. Возвращает HAL_OK при успехе.
HAL_StatusTypeDef Read(uint16_t memAddress, uint8_t* dst, uint16_t length, uint32_t timeoutMs = 100u);

// Запись блока в EEPROM (с разбиением по страницам по 16 байт). Возвращает HAL_OK при успехе.
HAL_StatusTypeDef Write(uint16_t memAddress, const uint8_t* src, uint16_t length, uint32_t timeoutMs = 100u);

} // namespace EEPROM

#endif /* EEPROM_HPP_ */
