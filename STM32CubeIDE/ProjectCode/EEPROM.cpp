#include "EEPROM.hpp"

extern I2C_HandleTypeDef hi2c3;

namespace EEPROM
{
// Базовый адрес 0xA0; для каждого 256-байтного блока A10..A8 кодируются в device select.
static uint16_t s_baseDeviceAddress = kDefaultDeviceAddress;
static constexpr uint16_t kBlockSizeBytes = 256u;

static bool IsRangeValid(uint16_t memAddress, uint16_t length)
{
    if (length == 0u)
    {
        return true;
    }
    const uint32_t endExclusive = (uint32_t)memAddress + (uint32_t)length;
    return (endExclusive <= (uint32_t)kSizeBytes);
}

// 8-битный адрес HAL для байта memAddress: 0xA0 | ((A10..A8) << 1).
static uint16_t DeviceAddressForMem(uint16_t memAddress)
{
    const uint16_t blockBits = (uint16_t)((memAddress >> 8) & 0x07u);
    return (uint16_t)((s_baseDeviceAddress & 0xF0u) | (blockBits << 1));
}

static uint16_t WordAddress8(uint16_t memAddress)
{
    return (uint16_t)(memAddress & 0xFFu);
}

static HAL_StatusTypeDef WaitDeviceReady(uint16_t deviceAddress, uint32_t timeoutMs)
{
    // Внутренний цикл программирования M24C16 обычно до 5 мс; даём запас по повторам.
    return HAL_I2C_IsDeviceReady(&hi2c3, deviceAddress, 20u, timeoutMs);
}

bool Init(uint16_t deviceAddress, uint32_t timeoutMs)
{
    // Сохраняем только «семейство» 0xA0: биты A10..A8 выбираются по адресу памяти.
    s_baseDeviceAddress = (uint16_t)(deviceAddress & 0xF0u);
    return (WaitDeviceReady(DeviceAddressForMem(0u), timeoutMs) == HAL_OK);
}

HAL_StatusTypeDef Read(uint16_t memAddress, uint8_t* dst, uint16_t length, uint32_t timeoutMs)
{
    if (dst == nullptr)
    {
        return HAL_ERROR;
    }
    if (!IsRangeValid(memAddress, length))
    {
        return HAL_ERROR;
    }
    if (length == 0u)
    {
        return HAL_OK;
    }

    // Нельзя пересекать границу 256-байтного блока без смены device select (A10..A8).
    uint16_t offset = 0u;
    while (offset < length)
    {
        const uint16_t currentAddress = (uint16_t)(memAddress + offset);
        const uint16_t blockRemain = (uint16_t)(kBlockSizeBytes - (currentAddress % kBlockSizeBytes));
        const uint16_t remain = (uint16_t)(length - offset);
        const uint16_t chunk = (remain < blockRemain) ? remain : blockRemain;
        const uint16_t devAddr = DeviceAddressForMem(currentAddress);

        HAL_StatusTypeDef st = HAL_I2C_Mem_Read(
            &hi2c3,
            devAddr,
            WordAddress8(currentAddress),
            I2C_MEMADD_SIZE_8BIT,
            &dst[offset],
            chunk,
            timeoutMs);
        if (st != HAL_OK)
        {
            return st;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return HAL_OK;
}

HAL_StatusTypeDef Write(uint16_t memAddress, const uint8_t* src, uint16_t length, uint32_t timeoutMs)
{
    if (src == nullptr)
    {
        return HAL_ERROR;
    }
    if (!IsRangeValid(memAddress, length))
    {
        return HAL_ERROR;
    }
    if (length == 0u)
    {
        return HAL_OK;
    }

    uint16_t offset = 0u;
    while (offset < length)
    {
        const uint16_t currentAddress = (uint16_t)(memAddress + offset);
        const uint16_t pageOffset = (uint16_t)(currentAddress % kPageSizeBytes);
        const uint16_t pageRemain = (uint16_t)(kPageSizeBytes - pageOffset);
        const uint16_t remain = (uint16_t)(length - offset);
        const uint16_t chunk = (remain < pageRemain) ? remain : pageRemain;
        const uint16_t devAddr = DeviceAddressForMem(currentAddress);

        HAL_StatusTypeDef st = HAL_I2C_Mem_Write(
            &hi2c3,
            devAddr,
            WordAddress8(currentAddress),
            I2C_MEMADD_SIZE_8BIT,
            (uint8_t*)&src[offset],
            chunk,
            timeoutMs);
        if (st != HAL_OK)
        {
            return st;
        }

        st = WaitDeviceReady(devAddr, timeoutMs);
        if (st != HAL_OK)
        {
            return st;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return HAL_OK;
}

} // namespace EEPROM
