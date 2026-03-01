/*
 * DefrostControl.h
 * C API алгоритма управления разморозкой (включение/выключение, инициализация, периодическое обновление).
 */

#ifndef DEFROSTCONTROL_H
#define DEFROSTCONTROL_H

#include <stdint.h>

#define DEFROST_PHASE_COUNT 3
#define DEFROST_MAX_SENSOR_COUNT 16

typedef enum {
    DEFROST_PARAM_GROUP_SENSORS = 1,
    DEFROST_PARAM_GROUP_TEMPERATURE = 2,
    DEFROST_PARAM_GROUP_HUMIDITY = 3,
    DEFROST_PARAM_GROUP_PWM = 4
} DefrostParamGroup_t;

typedef enum {
    DEFROST_PARAM_TYPE_U8 = 1,
    DEFROST_PARAM_TYPE_U16 = 2,
    DEFROST_PARAM_TYPE_F32 = 3
} DefrostParamType_t;

typedef struct {
    float fishHotMax_C[DEFROST_PHASE_COUNT];
    float fishHotRateMax_Cps[DEFROST_PHASE_COUNT];
    float fishDeltaMax_C[DEFROST_PHASE_COUNT];
    float supplySet_C[DEFROST_PHASE_COUNT];
    float supplyMax_C[DEFROST_PHASE_COUNT];
    float returnTargetRH_percent[DEFROST_PHASE_COUNT];
    float leftRightTrimGain;
    float leftRightTrimMaxEq;
    float wDeadband_kgkg;
    uint16_t outDamperTimer_s;
    uint16_t outFanDelay_s;
    uint16_t outHold_s;
    uint16_t tenMinHold_s;
    uint16_t injMinHold_s;
    uint8_t sensorUseInDefrost[DEFROST_MAX_SENSOR_COUNT];
} DefrostParams_t;

typedef struct {
    uint8_t valueType;
    union {
        uint8_t u8;
        uint16_t u16;
        float f32;
    } value;
} DefrostParamValue_t;

#ifdef __cplusplus
extern "C" {
#endif

void DefrostControl_Init(void);
void DefrostControl_SetEnabled(uint8_t enabled);
void DefrostControl_Update1s(void);
uint32_t DefrostControl_GetRuntimeSeconds(void);
uint8_t DefrostControl_IsEnabled(void);
uint8_t DefrostControl_GetParam(uint8_t groupId, uint8_t paramId, DefrostParamValue_t *outValue);
uint8_t DefrostControl_SetParam(uint8_t groupId, uint8_t paramId, const DefrostParamValue_t *inValue);
uint8_t DefrostControl_GetGroup(uint8_t groupId, uint8_t page, uint8_t *outData, uint8_t outCapacity, uint8_t *outLength);
void DefrostControl_GetParams(DefrostParams_t *outParams);
uint8_t DefrostControl_SetParams(const DefrostParams_t *inParams);
void DefrostControl_SaveParams(void);
void DefrostControl_LoadParams(void);

#ifdef __cplusplus
}
#endif

#endif /* DEFROSTCONTROL_H */
