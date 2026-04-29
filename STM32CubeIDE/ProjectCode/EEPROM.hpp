#ifndef EEPROM_HPP_
#define EEPROM_HPP_

#include "main.h"
#include <stdint.h>

namespace EEPROM
{
// Параметры M24C02.
static constexpr uint16_t kSizeBytes = 256u;
static constexpr uint16_t kPageSizeBytes = 8u;
static constexpr uint16_t kDefaultDeviceAddress = 0xA0u; // 8-битный адрес HAL при E2..E0=000.

// Инициализация драйвера: сохраняет адрес устройства и проверяет доступность EEPROM на шине.
bool Init(uint16_t deviceAddress = kDefaultDeviceAddress, uint32_t timeoutMs = 100u);

// Чтение блока из EEPROM. Возвращает HAL_OK при успехе.
HAL_StatusTypeDef Read(uint16_t memAddress, uint8_t* dst, uint16_t length, uint32_t timeoutMs = 100u);

// Запись блока в EEPROM (с разбиением по страницам по 8 байт). Возвращает HAL_OK при успехе.
HAL_StatusTypeDef Write(uint16_t memAddress, const uint8_t* src, uint16_t length, uint32_t timeoutMs = 100u);

} // namespace EEPROM

#endif /* EEPROM_HPP_ */
