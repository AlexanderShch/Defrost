/*
 * DefrostControl.h
 * C API алгоритма управления разморозкой (включение/выключение, инициализация, периодическое обновление).
 */

#ifndef DEFROSTCONTROL_H
#define DEFROSTCONTROL_H

#include <stdint.h>

#define DEFROST_PHASE_COUNT 3
#define DEFROST_MAX_SENSOR_COUNT 6   /* в алгоритме участвуют только датчики 0..5 */

typedef enum {
    DEFROST_PARAM_GROUP_SENSORS = 1,
    DEFROST_PARAM_GROUP_TEMPERATURE = 2,
    DEFROST_PARAM_GROUP_HUMIDITY = 3,
    DEFROST_PARAM_GROUP_PWM = 4,
    DEFROST_PARAM_GROUP_LOG_PHASE = 5,   /* группа 1 лога: параметры, зависящие от фазы (по запросу REQ_CMD_GET_DEFROST_GROUP) */
    DEFROST_PARAM_GROUP_LOG_GLOBAL = 6    /* группа 2 лога: параметры, общие для всех фаз (по запросу REQ_CMD_GET_DEFROST_GROUP) */
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
    /* Режим «только по воздуху» (без датчиков продукта): фаза по времени, макс. длительность, с */
    uint16_t airOnlyPhaseWarmUp_s;   /* длительность фазы WarmUp, с (например 600 = 10 мин) */
    uint16_t airOnlyPhasePlateau_s; /* конец фазы Plateau от старта, с (например 1800 = 30 мин) */
    uint16_t maxRuntime_s;           /* макс. длительность процесса, с (например 7200 = 2 ч); после — нагрев отключается */
    float fishColdTarget_C;          /* целевая мин. температура рыбы, °C; при достижении алгоритм останавливается */
    uint8_t debugDisableTargetTStop; /* 1 = не останавливать алгоритм по fishColdTarget_C (отладка), 0 = обычный автостоп */
    uint8_t debugDisableDeviceSwitchCheck; /* 1 = отключить проверку соответствия входов/выходов (отладка), 0 = проверка включена */
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
/** Принять и применить группу параметров (payload как в ответе GET_DEFROST_GROUP). groupId 5 или 6. */
uint8_t DefrostControl_SetGroupPayload(uint8_t groupId, const uint8_t *payload, uint8_t payloadLen);
void DefrostControl_GetParams(DefrostParams_t *outParams);
uint8_t DefrostControl_SetParams(const DefrostParams_t *inParams);
void DefrostControl_SaveParams(void);
void DefrostControl_LoadParams(void);
/** Целевая мин. температура рыбы °C (для автоостанова). Чтение/запись с экрана Settings1 (ValueCoreTSet). */
float DefrostControl_GetFishColdTarget_C(void);
void DefrostControl_SetFishColdTarget_C(float val_C);
uint8_t DefrostControl_IsDeviceSwitchCheckEnabled(void);

/* Ответ GET_DEFROST_GROUP(groupId=5): фиксированная структура — копирование памяти, без TLV. */
typedef struct __attribute__((packed)) {
    float fishHotMax_C[DEFROST_PHASE_COUNT];
    float fishHotRateMax_Cps[DEFROST_PHASE_COUNT];
    float fishDeltaMax_C[DEFROST_PHASE_COUNT];
    float supplySet_C[DEFROST_PHASE_COUNT];
    float supplyMax_C[DEFROST_PHASE_COUNT];
    float returnTargetRH_percent[DEFROST_PHASE_COUNT];
} DefrostLogPhasePayload_t;

/* Ответ GET_DEFROST_GROUP(groupId=6): фиксированная структура — копирование памяти, без TLV. Порядок полей совпадает с DefrostParams_t. */
typedef struct __attribute__((packed)) {
    float leftRightTrimGain;
    float leftRightTrimMaxEq;
    float piKp;
    float piKi;
    float wDeadband_kgkg;
    float injGain;
    uint16_t outDamperTimer_s;
    uint16_t outFanDelay_s;
    uint16_t outHold_s;
    uint16_t tenMinHold_s;
    uint16_t injMinHold_s;
    uint16_t airOnlyPhaseWarmUp_s;
    uint16_t airOnlyPhasePlateau_s;
    uint16_t maxRuntime_s;
    float fishColdTarget_C;          /* целевая мин. Т рыбы, °C; при fishCold_C >= fishColdTarget_C алгоритм останавливается */
    uint8_t debugDisableTargetTStop; /* 1 = не останавливать по fishColdTarget_C (отладка), 0 = автостоп включен */
    uint8_t debugDisableDeviceSwitchCheck; /* 1 = отключить проверку входов/выходов (отладка), 0 = проверка включена */
    uint8_t sensorUseInDefrost[DEFROST_MAX_SENSOR_COUNT];
} DefrostLogGlobalPayload_t;

/* Регулярный лог (Type 0x01): сначала отфильтрованные температуры всех датчиков (°C),
 * затем текущая фаза + группа 3 — переменные алгоритма.
 * Группы 1 и 2 — по запросу REQ_CMD_GET_DEFROST_GROUP (groupId 5 и 6). */
typedef struct __attribute__((packed)) {
    float T_filt_C[6];           /* 0..5: отфильтрованные T датчиков (см. Sensor_array индекс) */
    uint8_t phase;               /* 0=WarmUp, 1=Plateau, 2=Finish */
    float eT_common, heatScale01;
    float uCommon_TEN, trim_TEN, uLeft_TEN, uRight_TEN;
    float leftTen1Duty, leftTen2Duty, rightTen1Duty, rightTen2Duty;
    float w_sup_avg, wErr, injDuty;
    float rate_Cps;
    float fishHot_C, fishCold_C;
} ControlLogPayload_t;

ControlLogPayload_t DefrostControl_GetControlLogPayload(void);

#ifdef __cplusplus
}
#endif

#endif /* DEFROSTCONTROL_H */
