#include "EEPROM.hpp"
#include "cmsis_os2.h"

extern I2C_HandleTypeDef hi2c3;

namespace EEPROM
{
static osMutexId_t s_mutex = nullptr;

bool Lock()
{
    const osKernelState_t state = osKernelGetState();
    if (state != osKernelRunning && state != osKernelLocked) return true;
    const int32_t previous = osKernelLock();
    if (s_mutex == nullptr)
    {
        const osMutexAttr_t attributes = {"I2C3", osMutexRecursive | osMutexPrioInherit, nullptr, 0u};
        s_mutex = osMutexNew(&attributes);
    }
    osKernelRestoreLock(previous);
    return s_mutex != nullptr && osMutexAcquire(s_mutex, osWaitForever) == osOK;
}

void Unlock()
{
    const osKernelState_t state = osKernelGetState();
    if ((state == osKernelRunning || state == osKernelLocked) && s_mutex != nullptr)
        osMutexRelease(s_mutex);
}

class AccessGuard
{
public:
    AccessGuard() : acquired(Lock()) {}
    ~AccessGuard() { if (acquired) Unlock(); }
    bool acquired;
};

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
    // Count elapsed time, not immediate ACK attempts: a page write can take 5 ms.
    const uint32_t started = HAL_GetTick();
    for (;;)
    {
        const uint32_t elapsed = HAL_GetTick() - started;
        const uint32_t remaining = elapsed < timeoutMs ? timeoutMs - elapsed : 0u;
        const uint32_t probeTimeout = remaining < 5u ? remaining : 5u;
        if (HAL_I2C_IsDeviceReady(&hi2c3, deviceAddress, 1u, probeTimeout) == HAL_OK)
            return HAL_OK;
        if (HAL_GetTick() - started >= timeoutMs) return HAL_TIMEOUT;
        const osKernelState_t state = osKernelGetState();
        if (state == osKernelRunning)
        {
            const uint32_t ticks = (osKernelGetTickFreq() + 999u) / 1000u;
            osDelay(ticks);
        }
        else HAL_Delay(1u);
    }
}

bool Init(uint16_t deviceAddress, uint32_t timeoutMs)
{
    AccessGuard guard;
    if (!guard.acquired) return false;
    // Сохраняем только «семейство» 0xA0: биты A10..A8 выбираются по адресу памяти.
    s_baseDeviceAddress = (uint16_t)(deviceAddress & 0xF0u);
    return (WaitDeviceReady(DeviceAddressForMem(0u), timeoutMs) == HAL_OK);
}

HAL_StatusTypeDef Read(uint16_t memAddress, uint8_t* dst, uint16_t length, uint32_t timeoutMs)
{
    AccessGuard guard;
    if (!guard.acquired) return HAL_BUSY;
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
    AccessGuard guard;
    if (!guard.acquired) return HAL_BUSY;
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

// The touch controller and EEPROM share I2C3, including the same HAL handle.
extern "C" uint8_t I2C3_BusLock(void) { return EEPROM::Lock() ? 1u : 0u; }
extern "C" void I2C3_BusUnlock(void) { EEPROM::Unlock(); }
