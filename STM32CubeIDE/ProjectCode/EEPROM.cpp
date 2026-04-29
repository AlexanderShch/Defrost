#include "EEPROM.hpp"

extern I2C_HandleTypeDef hi2c3;

namespace EEPROM
{
static uint16_t s_deviceAddress = kDefaultDeviceAddress;

static bool IsRangeValid(uint16_t memAddress, uint16_t length)
{
    if (length == 0u)
    {
        return true;
    }
    const uint32_t endExclusive = (uint32_t)memAddress + (uint32_t)length;
    return (endExclusive <= (uint32_t)kSizeBytes);
}

static HAL_StatusTypeDef WaitDeviceReady(uint32_t timeoutMs)
{
    // Внутренний цикл программирования M24C02 обычно до 5 мс; даём запас по повторам.
    return HAL_I2C_IsDeviceReady(&hi2c3, s_deviceAddress, 20u, timeoutMs);
}

bool Init(uint16_t deviceAddress, uint32_t timeoutMs)
{
    s_deviceAddress = deviceAddress;
    return (WaitDeviceReady(timeoutMs) == HAL_OK);
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

    return HAL_I2C_Mem_Read(
        &hi2c3,
        s_deviceAddress,
        memAddress,
        I2C_MEMADD_SIZE_8BIT,
        dst,
        length,
        timeoutMs);
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

        HAL_StatusTypeDef st = HAL_I2C_Mem_Write(
            &hi2c3,
            s_deviceAddress,
            currentAddress,
            I2C_MEMADD_SIZE_8BIT,
            (uint8_t*)&src[offset],
            chunk,
            timeoutMs);
        if (st != HAL_OK)
        {
            return st;
        }

        st = WaitDeviceReady(timeoutMs);
        if (st != HAL_OK)
        {
            return st;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return HAL_OK;
}

} // namespace EEPROM
