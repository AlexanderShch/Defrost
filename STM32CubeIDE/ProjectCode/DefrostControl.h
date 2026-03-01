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
    /* Ограничения по температуре продукта и подачи [фаза 0=WarmUp, 1=Plateau, 2=Finish] */
    float fishHotMax_C[DEFROST_PHASE_COUNT];         /* потолок температуры самой тёплой точки продукта, °C */
    float fishHotRateMax_Cps[DEFROST_PHASE_COUNT];   /* потолок скорости прогрева «горячей» точки, °C/с */
    float fishDeltaMax_C[DEFROST_PHASE_COUNT];       /* потолок разницы горячая–холодная точка продукта, °C */
    float supplySet_C[DEFROST_PHASE_COUNT];         /* уставка температуры воздуха подачи, °C */
    float supplyMax_C[DEFROST_PHASE_COUNT];         /* потолок температуры воздуха в потоках подачи, °C */
    float returnTargetRH_percent[DEFROST_PHASE_COUNT]; /* уставка относительной влажности в возврате, % */
    /* Балансировка левый/правый поток */
    float leftRightTrimGain;   /* коэффициент подстройки по разности температур подачи лево/право */
    float leftRightTrimMaxEq;   /* макс. эквивалент ТЭНа для коррекции дисбаланса (0..2) */
    /* ПИ-регулятор температуры подачи */
    float piKp;                /* пропорциональный коэффициент */
    float piKi;                /* интегральный коэффициент */
    /* Влажность: форсунка и мёртвая зона */
    float wDeadband_kgkg;       /* мёртвая зона по абсолютной влажности, кг/кг (чтобы не дёргать форсунку и вытяжку) */
    float injGain;              /* коэффициент форсунки: скважность на (кг/кг) по ошибке влажности */
    /* Тайминги вытяжки и актуаторов, с */
    uint16_t outDamperTimer_s;  /* время полного открытия заслонки вытяжки */
    uint16_t outFanDelay_s;     /* задержка включения вентилятора вытяжки после открытия заслонки */
    uint16_t outHold_s;         /* минимальное время удержания состояния вытяжки (вкл/выкл) */
    uint16_t tenMinHold_s;      /* минимальное время удержания состояния ТЭНов (вкл/выкл) */
    uint16_t injMinHold_s;      /* минимальное время удержания состояния форсунки (вкл/выкл) */
    uint8_t sensorUseInDefrost[DEFROST_MAX_SENSOR_COUNT]; /* 1=использовать датчик в дефросте, 0=игнорировать */
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
