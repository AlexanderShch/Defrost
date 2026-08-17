/*
 * DefrostControl.cpp
 *
 * Практичный алгоритм управления дефростером с двумя потоками:
 * - Два потока подачи (левый/правый) с 2 ТЭН + 2 вентилятора на сторону
 * - Одна точка возврата/смешения (чердак над всасывающим люком)
 * - Форсунки с двух сторон (впрыск воды) и вытяжка для управления влажностью
 *
 * Цели:
 * - Быстро размораживать без ухудшения качества (не перегревать / не портить поверхность)
 * - Балансировать левую/правую стороны, чтобы не было перекоса
 * - Управлять влажностью через абсолютную влажность (устойчивее, чем RH)
 *
 * ВАЖНО:
 * - Модуль сам по себе не активируется. Внешний код должен вызвать DefrostControl_SetEnabled(1),
 *   когда можно безопасно передать управление актуаторами.
 * - Уставки и коэффициенты — стартовые/консервативные. Их нужно настраивать на реальном оборудовании.
 */
 
 #include <math.h>
 #include <stdint.h>
 #include <string.h>
 
 #include "Data.hpp"                 // индексы датчиков SQ
 #include "DefrostControl.h"
#include "EEPROM.hpp"
 #include "GateControl.hpp"
#include "main.h"
 #include <gui/model/Model.hpp>      // Model::getCurrentVal_H, Model::DFR (T для алгоритма — Sensor Param 4)
 #include "ModBus.hpp"

#define DEFAULT_DEBUG_DISABLE_TARGET_T_STOP  0u   /* 0 = автостоп по целевой Т включен (по умолчанию) */

extern SENSOR_typedef_t Sensor_array[SQ];
extern osSemaphoreId_t SensorsReadDone_SemHandle;
extern unsigned int TimeFromStart;

/* Буфер параметров для лога (заполняется в ControlStep1s/ControlStep1s_AirOnly, читается из Data.cpp). */
static ControlLogPayload_t s_controlLogPayload;
static_assert(sizeof(ControlLogPayload_t) == 89, "ControlLogPayload_t size must be 89 bytes");
/* Текущая фаза для ответа по REQ_CMD_GET_DEFROST_GROUP groupId 5 (группа 1 лога). */
static uint8_t s_lastPhase = 0;

// структура для хранения параметров в EEPROM
typedef struct
{
    uint16_t version; // версия структуры
    uint8_t autoModeEnabled; // бит, определяющий, включен ли режим авто-дефроста
    uint8_t reserved; // резервные байты
    DefrostParams_t params; // параметры дефростера
    uint16_t payloadCrc; // CRC записи в EEPROM параметров
} DefrostEepromStorage_t;

static DefrostParams_t g_defrostParams; // рабочие параметры дефростера
static const uint8_t kDefrostSensorCount = (SQ < DEFROST_MAX_SENSOR_COUNT) ? SQ : DEFROST_MAX_SENSOR_COUNT;
static const uint16_t kDefrostEepromVersion = 1u; // версия структуры в EEPROM
static const uint16_t kDefrostEepromBaseAddress = 0u; // адрес начала записи в EEPROM
static bool g_defrostEepromAvailable = false; // флаг, определяющий, доступна ли EEPROM
static uint8_t g_defrostPersistedAutoMode = 0u; // бит, определяющий, включен ли режим авто-дефроста
/* Отложенная запись в EEPROM: не пишем в LoadParams до старта RTOS (блокирующая запись
 * по страницам на I2C3 может надолго задержать main и сорвать запуск опроса датчиков). */
static bool g_defrostEepromPersistPending = false;
/* Восстановить авторежим после сбоя питания: StartAutomaticSequence — на первом Update1s (уже RTOS, есть DI). */
static uint8_t g_defrostAutoRestorePending = 0u;
static_assert(sizeof(DefrostEepromStorage_t) <= EEPROM::kSizeBytes, "Defrost EEPROM payload exceeds M24C16 capacity");
 
 namespace
 {
     // ─────────────────────────────────────────────────────────────────────────────
     // Привязка индексов датчиков к физическим точкам.
     // Почему: алгоритму нужна явная карта "канал телеметрии → физическая точка".
     //          Значения по умолчанию соответствуют Sensor_array в ModBus.cpp (0=левый поток,1=правый,2=возврат,3&4=рыба).
     // ─────────────────────────────────────────────────────────────────────────────
     constexpr int8_t kSensSupLeft_T_H  = 0;
     constexpr int8_t kSensSupRight_T_H = 1;
     constexpr int8_t kSensReturn_T_H   = 2;
     constexpr int8_t kSensFish1_T      = 3;
     constexpr int8_t kSensFish2_T      = 4;
     // Device_AlarmFlags: бит 13 — выпадение датчика продукта 3, бит 14 — датчика 4.
     constexpr uint16_t kProductFallenLeftBit  = (uint16_t)(1u << 13);
     constexpr uint16_t kProductFallenRightBit = (uint16_t)(1u << 14);
     constexpr float kProductReinsertMatch_C = 3.0f;

     /** Температура для расчётов автоматики: Param 4 — усреднение по буферу + ограничение скорости (Clatch), см. Data.cpp.
      *  В Model::setCurrentVal_T попадает сырая T с шины (Param 2) — для экрана; для ControlStep1s нужен Param 4, как в логе T_filt_C. */
     inline int16_t FilteredSensorT_Deci(int8_t sensorIndex)
     {
         return (int16_t)Sensor::GetData(TimeFromStart, (unsigned char)sensorIndex, 4);
     }

     /** Бит аварии датчика (обрезки) или недоступность канала для контура управления. */
     inline bool SensorExcludedByClampAlarm(int8_t sensorIndex)
     {
         if (sensorIndex < 0)
             return true;
         return (Model::Sensor_AlarmFlags & (1u << (unsigned char)sensorIndex)) != 0;
     }

     /** Левый/правый/возврат дефростера (индексы 0..2): участвует в автоматике, если датчик активен на шине и нет аварии обрезок. */
     inline bool DefrostAirThChannelUsable(int8_t idx)
     {
         if (idx < kSensSupLeft_T_H || idx > kSensReturn_T_H)
             return false;
        return (Sensor_array[idx].Active == 1) &&
               (Sensor_array[idx].UseInDefrost != 0) &&
               !SensorExcludedByClampAlarm(idx);
     }

     // Масштаб "в десятых" (UI делит на 10.0).
     // Почему: преобразование должно быть в одинаковых единицах, чтобы не смешивать "сырые" и инженерные единицы.
     constexpr float kDeciToUnit = 0.1f;
     inline float DeciToC(int16_t deciC)  { return (float)deciC * kDeciToUnit; }
     inline float DeciToRH(int16_t deciRH) { return (float)deciRH * kDeciToUnit; }

     /** Допущение при отсутствии обеих Т подачи, но исправном возврате: оценка T подачи = T_возврата + смещение, °C. */
     constexpr float kSupplyTInferOffsetFromReturn_C = 2.0f;

     /** Входы воздушного контура для ПИ: средние и подстановки при отключённых датчиках Т подачи/возврата. */
     struct DefrostAirControlInputs
     {
         float mL, mR, mRet;   // отфильтрованные T, °C
         uint8_t okL, okR, okRet;
         float T_sup_avg_C;    // среднее по исправным подачам; при отказе обеих подач и исправном возврате — mRet + kSupplyTInferOffsetFromReturn_C; иначе 0
         float T_ret_C;        // возврат или при отказе — T_sup_avg_C
         float RH_sup_avg;     // RH для w_sup: подачи или при их отсутствии — RH возврата; при полном отказе 0..2 — 50 % (не для контура форсунки/вытяжки)
         uint8_t humidityMeasOk; // 1 = есть хотя бы один канал 0..2 для контура влажности
     };

     // вычисление входных данных для управления дефростом и заполнение структуры DefrostAirControlInputs
     // a - указатель на структуру DefrostAirControlInputs
     // учитываются только исправные датчики воздушного потока и датчик возврата
     inline void ComputeDefrostAirControlInputs(DefrostAirControlInputs* a)
     {
         a->mL   = DeciToC(FilteredSensorT_Deci(kSensSupLeft_T_H)); // получаем значения с левого датчика воздушного потока
         a->mR   = DeciToC(FilteredSensorT_Deci(kSensSupRight_T_H)); // получаем значения с правого датчика воздушного потока
         a->mRet = DeciToC(FilteredSensorT_Deci(kSensReturn_T_H)); // получаем значения с датчика возврата
         a->okL   = DefrostAirThChannelUsable(kSensSupLeft_T_H) ? 1u : 0u; // проверяем, исправен ли левый датчик воздушного потока
         a->okR   = DefrostAirThChannelUsable(kSensSupRight_T_H) ? 1u : 0u; // проверяем, исправен ли правый датчик воздушного потока
         a->okRet = DefrostAirThChannelUsable(kSensReturn_T_H) ? 1u : 0u; // проверяем, исправен ли датчик возврата

         const unsigned nSup = (unsigned)a->okL + (unsigned)a->okR; // считаем количество исправных датчиков воздушного потока
         if (nSup > 0u) // если есть исправные датчики воздушного потока, то среднее значение температуры воздушного потока равно среднему значению температуры с левого и правого датчиков воздушного потока
             a->T_sup_avg_C = ((a->okL ? a->mL : 0.0f) + (a->okR ? a->mR : 0.0f)) / (float)nSup; // среднее значение температуры воздушного потока
         // НЕТ исправных датчиков воздушного потока, но есть исправный датчик возврата
         else if (a->okRet != 0u)
            // если датчик возврата исправен, то среднее значение температуры воздушного потока равно значению датчика возврата плюс смещение    
            a->T_sup_avg_C = a->mRet + kSupplyTInferOffsetFromReturn_C; 
         // НЕТ исправных датчиков воздушного потока и НЕТ исправного датчика возврата
         else 
            // если все датчики воздушного потока НЕисправны, то среднее значение температуры воздушного потока равно 0
            a->T_sup_avg_C = 0.0f;  // если =0, то heatScale01 принудительно обнуляется — нагрев по воздушному контуру отключается

         if (a->okRet != 0u) 
            // если датчик возврата исправен, то температура возврата равна значению датчика возврата
            a->T_ret_C = a->mRet;
         else 
            // если датчик возврата НЕисправен, то температура возврата равна среднему значению температуры воздушного потока
            a->T_ret_C = a->T_sup_avg_C;

         const float RH_supL = DeciToRH((int16_t)Model::getCurrentVal_H(kSensSupLeft_T_H)); // получаем значение относительной влажности с левого датчика воздушного потока
         const float RH_supR = DeciToRH((int16_t)Model::getCurrentVal_H(kSensSupRight_T_H)); // получаем значение относительной влажности с правого датчика воздушного потока
         if (a->okL != 0u && a->okR != 0u)
             a->RH_sup_avg = 0.5f * (RH_supL + RH_supR); // если оба датчика воздушного потока исправны, то среднее значение относительной влажности равно среднему значению относительной влажности с левого и правого датчиков воздушного потока
         else if (a->okL != 0u)
             a->RH_sup_avg = RH_supL; // если левый датчик воздушного потока исправен, то среднее значение относительной влажности равно значению относительной влажности с левого датчика воздушного потока
         else if (a->okR != 0u)
             a->RH_sup_avg = RH_supR; // если правый датчик воздушного потока исправен, то среднее значение относительной влажности равно значению относительной влажности с правого датчика воздушного потока
         else if (a->okRet != 0u)
             a->RH_sup_avg = DeciToRH((int16_t)Model::getCurrentVal_H(kSensReturn_T_H));
         else
             a->RH_sup_avg = 50.0f; // заглушка для лога; контур форсунки/вытяжки при полном отказе 0..2 не ведём
         a->humidityMeasOk = ((a->okL | a->okR | a->okRet) != 0u) ? 1u : 0u;
     }
 
     // Допущения по психрометрии.
     // Почему: без датчика давления для расчёта абсолютной влажности приходится принимать номинальное давление.
     constexpr float kAirPressure_kPa = 101.325f;
 
     // Период обновления управления.
     // Почему: алгоритм рассчитан на дискретность 1 сек и использует ограничения по скорости изменения.
     constexpr float kDt_s = 1.0f;
 
     // Ограничения безопасности/качества (по умолчанию; требуют настройки под продукт).
     // Почему: ограничения качества должны доминировать над "оптимальностью", чтобы избежать необратимых дефектов.
     // Снимок лимитов для одной из фаз (0=WarmUp, 1=Plateau, 2=Finish); значения берутся из GetLimits(phase).
     struct Limits
     {
         float fishHotMax_C;        // жёсткий потолок для самой тёплой измеренной точки продукта
         float fishHotRateMax_Cps;  // потолок скорости прогрева "горячей" точки (°C/с)
         float fishDeltaMax_C;      // потолок неравномерности (горячая - холодная)
         float supplyMax_C;         // потолок температуры воздуха в потоках подачи
     };
 
     // Уставки, зависящие от фазы (по умолчанию; настройка под продукт и аэродинамику).
     // Почему: около 0°C доминирует скрытая теплота плавления, динамика меняется — уставки должны адаптироваться.
     struct Targets
     {
         float supplySet_C;             // основная уставка температуры подачи (на обе стороны)
         float returnTargetRH_percent;  // уставка влажности как RH при T возврата (далее переводится в абсолютную влажность)
     };

    // CRC записи EEPROM: тот же ModBus CRC16, что MB_GetCRC (датчики / сервер).
    static uint16_t EepromPayloadCrc(const DefrostEepromStorage_t *rec)
    {
        uint8_t payload[1u + sizeof(DefrostParams_t)] = {};
        payload[0] = rec->autoModeEnabled;
        memcpy(&payload[1], &rec->params, sizeof(DefrostParams_t));
        return MB_GetCRC((volatile uint8_t *)payload, (uint16_t)sizeof(payload));
    }

    // сохранение параметров в EEPROM, если EEPROM доступна
    static void PersistParamsToEepromIfAvailable(void)
    {
        if (!g_defrostEepromAvailable) return;
        DefrostEepromStorage_t rec = {};
        rec.version = kDefrostEepromVersion;
        rec.autoModeEnabled = g_defrostPersistedAutoMode;
        rec.reserved = 0u;
        memcpy(&rec.params, &g_defrostParams, sizeof(DefrostParams_t));
        rec.payloadCrc = EepromPayloadCrc(&rec);
        if (EEPROM::Write(kDefrostEepromBaseAddress, (const uint8_t *)&rec, (uint16_t)sizeof(rec)) == HAL_OK)
        {
            g_defrostEepromPersistPending = false;
        }
        else
        {
            // Повторим на следующем тике Update1s.
            g_defrostEepromPersistPending = true;
        }
    }

    static void FlushPendingEepromPersistIfNeeded(void)
    {
        if (g_defrostEepromPersistPending)
        {
            PersistParamsToEepromIfAvailable();
        }
    }
    // загрузка параметров из EEPROM, если EEPROM доступна
    static void LoadDefaultParams(DefrostParams_t *p)
    {
        memset(p, 0, sizeof(DefrostParams_t));

        p->fishHotMax_C[0] = 20.0f;
        p->fishHotMax_C[1] = 20.0f;
        p->fishHotMax_C[2] = 20.0f;
        p->fishHotRateMax_Cps[0] = 0.020f;
        p->fishHotRateMax_Cps[1] = 0.015f;
        p->fishHotRateMax_Cps[2] = 0.010f;
        p->fishDeltaMax_C[0] = 6.0f;
        p->fishDeltaMax_C[1] = 5.0f;
        p->fishDeltaMax_C[2] = 4.0f;
        p->supplySet_C[0] = 30.0f;
        p->supplySet_C[1] = 26.0f;
        p->supplySet_C[2] = 22.0f;
        p->supplyMax_C[0] = 35.0f;
        p->supplyMax_C[1] = 30.0f;
        p->supplyMax_C[2] = 26.0f;
        p->returnTargetRH_percent[0] = 85.0f;
        p->returnTargetRH_percent[1] = 92.0f;
        p->returnTargetRH_percent[2] = 85.0f;
        p->leftRightTrimGain = 0.08f;
        p->leftRightTrimMaxEq = 0.6f;
        p->piKp = 0.18f;
        p->piKi = 0.02f;
        p->wDeadband_kgkg = 0.0008f;
        p->injGain = 900.0f;
        p->outDamperTimer_s = 10u;
        p->outFanDelay_s = 5u;
        p->outHold_s = 15u;
        p->tenMinHold_s = 10u;
        p->injMinHold_s = 5u;
        p->airOnlyPhaseWarmUp_s = 600u;   /* 10 мин */
        p->airOnlyPhasePlateau_s = 1800u; /* 30 мин от старта */
        p->maxRuntime_s = 7200u;           /* 2 ч */
        p->fishColdTarget_C = 6.0f;        /* целевая мин. Т рыбы °C; при достижении — автоостанов алгоритма */
        p->debugDisableTargetTStop = (uint8_t)DEFAULT_DEBUG_DISABLE_TARGET_T_STOP;
        p->debugDisableDeviceSwitchCheck = 0u; /* по умолчанию проверка DO->DI включена */

        for (uint8_t i = 0; i < kDefrostSensorCount; i++)
        {
            p->sensorUseInDefrost[i] = Sensor_array[i].UseInDefrost;
        }
    }

    static uint8_t IsPhaseIndexValid(uint8_t phaseIndex)
    {
        return (phaseIndex < DEFROST_PHASE_COUNT) ? 1u : 0u;
    }
 
     enum class Phase : uint8_t
     {
         WarmUp = 0,   // продукт заметно ниже 0°C
         Plateau = 1,  // около 0°C, доминирует скрытая теплота
         Finish = 2,   // выше 0°C, мягкое доведение
     };
 
     static Phase SelectPhase(float fishCold_C)
     {
         // Почему: пороги около 0°C описывают область фазового перехода; держим их явными и легко настраиваемыми.
         if (fishCold_C < -2.0f) return Phase::WarmUp;
         if (fishCold_C <  1.0f) return Phase::Plateau;
         return Phase::Finish;
     }

     // Режим «только по воздуху»: фаза по времени из параметров.
     static Phase SelectPhaseByTime(uint32_t runtime_s)
     {
         const uint32_t tWarmUp  = (uint32_t)g_defrostParams.airOnlyPhaseWarmUp_s;
         const uint32_t tPlateau = (uint32_t)g_defrostParams.airOnlyPhasePlateau_s;
         if (runtime_s < tWarmUp)  return Phase::WarmUp;
         if (runtime_s < tPlateau) return Phase::Plateau;
         return Phase::Finish;
     }

     static void ControlStep1s_AirOnly();
     static void StepExhaustByHumidityError(float wErr);

     static Limits GetLimits(Phase p)
     {
        uint8_t idx = 0;
        if (p == Phase::Plateau) idx = 1;
        else if (p == Phase::Finish) idx = 2;

        return Limits{
            g_defrostParams.fishHotMax_C[idx],
            g_defrostParams.fishHotRateMax_Cps[idx],
            g_defrostParams.fishDeltaMax_C[idx],
            g_defrostParams.supplyMax_C[idx]
        };
     }
 
     static Targets GetTargets(Phase p)
     {
        uint8_t idx = 0;
        if (p == Phase::Plateau) idx = 1;
        else if (p == Phase::Finish) idx = 2;

        return Targets{
            g_defrostParams.supplySet_C[idx],
            g_defrostParams.returnTargetRH_percent[idx]
        };
     }
 
     static float Clamp(float x, float lo, float hi)
     {
         if (x < lo) return lo;
         if (x > hi) return hi;
         return x;
     }
 
     static float SaturationVaporPressure_kPa(float T_C)
     {
         // Аппроксимация Магнуса–Тетенса (хватает для HVAC-диапазонов).
         // Почему: даёт хорошую точность при малых вычислениях, достаточно для управления (не для поверки).
         const float a = 17.625f;
         const float b = 243.04f;
         const float gamma = (a * T_C) / (b + T_C);
         return 0.61094f * expf(gamma);
     }
 
     static float HumidityRatio_kgkg(float T_C, float RH_percent)
     {
         const float RH = Clamp(RH_percent, 0.0f, 100.0f) * 0.01f;
         const float es_kPa = SaturationVaporPressure_kPa(T_C);
         const float pv_kPa = RH * es_kPa;
 
         // Защита от деления на ноль/отрицательных значений при pv→P.
         // Почему: выбросы датчика при RH~100% иначе могут раскачивать регулятор.
         const float denom = Clamp(kAirPressure_kPa - pv_kPa, 0.5f, kAirPressure_kPa);
         return 0.62198f * (pv_kPa / denom);
     }
 
     // Пропорционально - интегральное управление
     struct PI
     {
         float kp = 0.0f;       // пропорциональный коэффициент
         float ki = 0.0f;       // интегральный коэффициент
         float i = 0.0f;        // интегральный накопитель ошибки
 
         float Step(float error, float dt, float uMin, float uMax)    // расчёт управляющего воздействия
         {
             // Ограничение интегратора при насыщении: корректируем интегратор при насыщении выхода.
             // Почему: актуаторы насыщаются (0..2 "эквивалента ТЭНа"), разгон интегратора даёт длинные перерегулирования.
             const float p = kp * error;    // пропорциональная составляющая
             i += ki * error * dt;          // интегральная составляющая
             float u = p + i;               // суммарное воздействие
             if (u > uMax)                  // ограничение выхода за верхнюю границу
             {
                 u = uMax;
                 i = u - p;                 // корректировка интегральной составляющей - рост не учитывается
             }
             else if (u < uMin)             // ограничение выхода за нижнюю границу
             {
                 u = uMin;
                 i = u - p;                 // корректировка интегральной составляющей - снижение не учитывается
             }
             return u;
         }
     };
 
     // Расчёт скважности для ШИМ методом сигма-дельта
     struct SigmaDeltaPWM
     {
         float acc = 0.0f;                  // накопитель
         uint8_t Step(float duty01)         // duty01 — заданная скважность 0…1 (0 = выкл, 1 = вкл)
         {
             // При обновлении раз в секунду выход принимает значения только 0 или 1; 
             // накопитель распределяет включения по тактам так, чтобы средняя скважность за период 
             // совпадала с заданной (эквивалент ШИМ).
             duty01 = Clamp(duty01, 0.0f, 1.0f);
             acc += duty01;                 // накопитель скважности
             if (acc >= 1.0f)               // накопитель превысил 100%, нужно включить устройство
             {
                 acc -= 1.0f;               // устройство включаем, накопитель сбрасываем на 100%
                 return 1;                  // возвращаем команду на включение
             }
             return 0;                      // накопитель ещё не полный, устройство включать не нужно
         }
 
         void Reset()
         {
             acc = 0.0f;
         }
     };
 
     // Удержание объекта от переключения в течение не менее заданного времени
     struct HoldSwitch
     {
         uint8_t state = 0;
         uint16_t hold_s = 0;
 
         uint8_t Step(uint8_t desiredState, uint16_t minHold_s)
         {
             // Почему: 1-секундная дискретность + sigma-delta может вызвать частые переключения,
             //         что плохо для силовых реле/контакторов и ухудшает повторяемость теплового режима.
             // Гарантируем минимум 1 с, иначе при minHold_s==0 переключение возможно каждый тик.
             if (minHold_s == 0u)
                 minHold_s = 1u;
             if (hold_s > 0)
             {
                 hold_s--;
                 return state;
             }
             // Переход к переключению состояния может быть только по истечении минимального времени удержания
             desiredState = desiredState ? 1 : 0;
             if (desiredState != state)
             {
                 state = desiredState;
                 hold_s = minHold_s;
             }
             return state;
             // Важно: DefrostControl_Update1s() должен вызываться ровно 1 раз в секунду (DataTimerFunc 1 Гц).
             // При более частом вызове hold_s будет уменьшаться быстрее и удержание сократится.
         }
 
         void Reset(uint8_t initialState = 0)
         {
             state = initialState ? 1 : 0;
             hold_s = 0;
         }
     };

     struct SideActuators
     {
         // Два ТЭНа на сторону.
         SigmaDeltaPWM ten1;
         SigmaDeltaPWM ten2;
         HoldSwitch ten1Hold;
         HoldSwitch ten2Hold;
     };
 
     struct ControllerState
     {
         // Флаг включения(1)/выключения(0) автоматического управления контроллером
         uint8_t enabled = 0;
        uint32_t runtimeSeconds = 0;
         // Структура с параметрами ПИ-регулирования
         PI piSupplyCommon;
         // (эквивалент ТЭНа) на °C дисбаланса Т входящего потока воздуха
         float leftRightTrimGain = 0.08f;
 
         // Влажность: мёртвая зона нужна, чтобы не "драться" форсунке и вытяжке.
         float wDeadband_kgkg = 0.0008f;   // ~0.8 г/кг сухого воздуха
         // Структура с параметрами ШИМ-модулятора форсунки
         SigmaDeltaPWM injPwm;
         // Структура с параметрами времени удержания форсунки
         HoldSwitch injHold;
 
        // Вытяжка: включение(1) / выключение (0)
        uint8_t outOn = 0;
        // Счётчик времени удержания вытяжки в текущем состоянии (включена/выключена) для предотвращения частых переключений
        uint16_t outHold_s = 0;

        // Последовательное управление заслонкой и вентилятором вытяжки
        uint8_t outDamperState = 0;     // 0=закрыта, 1=открывается, 2=открыта
        uint16_t outDamperTimer_s = 0;  // таймер открытия заслонки
        uint16_t outFanDelay_s = 5;     // задержка включения вентилятора после открытия заслонки
        uint8_t outFanOn = 0;           // состояние вентилятора вытяжки
        uint16_t shutdownOutFanRemain_s = 0; // остаток продувки при завершении; 0 до старта таймера после проверки Vent_Out

        uint8_t startupGateClosing = 0; // при старте: 1 если нужно закрыть ворота через API GateControl
        uint8_t shutdownActive = 0;     // при остановке: 1 пока выполняется последовательность остановки
       uint8_t shutdownGateOpening = 0; // при остановке: 1 если выполняется стартовый импульс открытия ворот
        uint8_t shutdownGateLiftPulse_s = 0; // длительность импульса открытия ворот в начале продувки (с)
       uint8_t shutdownGateFullOpenActive = 0; // при остановке: 1 пока идёт шаг FullGateOpen (ворота + закрытие заслонки)
       uint8_t shutdownGateOpenReleased = 0; // 1 — команда Open с ворот снята после завершения движения привода
       uint8_t shutdownGateOpenSawActive = 0; // 1 — в этом шаге уже была активна команда Open (защита от гонки с Arm)
       uint8_t shutdownGateOpenPhase_s = 0;   // секунды в шаге FullGateOpen (таймаут, если Open так и не стала активной)
       uint8_t shutdownStage = 0; // текущее состояние post-shutdown (см. ShutdownStage)
        uint16_t shutdownFlapOpenWait_s = 0; // без DeviceSwitchCheck: ожидание Air_Open в Ventilation; на FullGateOpen — ожидание закрытия по времени
        uint8_t startPendingAfterShutdown = 0; // START получен во время post-shutdown; запуск отложен до полного завершения останова
        uint8_t alarmBlinkPhase = 0;    // фаза мигания аварийной лампы: 0/1 (1 Гц)
        uint8_t startupActuatorDelay_s = 0; // пауза между последовательными включениями вентиляторов и ТЭНов
        uint8_t shutdownActuatorDelay_s = 0; // пауза между последовательными выключениями вентиляторов и ТЭНов
        uint16_t flapTransitionElapsed_s = 0; // длительность текущего перехода заслонки по концевикам
        uint8_t flapDesiredOpenPrev = 0;      // предыдущее заданное состояние заслонки (0/1)
        uint8_t flapDesiredOpenPrevValid = 0; // признак инициализации flapDesiredOpenPrev
        uint8_t flapAlarm = 0;                // авария заслонки: переход не завершился за таймаут
        uint8_t stagedVent1LeftOn = 0;
        uint8_t stagedVent2LeftOn = 0;
        uint8_t stagedVent1RightOn = 0;
        uint8_t stagedVent2RightOn = 0;
        uint8_t stagedTen1LeftOn = 0;
        uint8_t stagedTen2LeftOn = 0;
        uint8_t stagedTen1RightOn = 0;
        uint8_t stagedTen2RightOn = 0;
         // Структуры ШИМ-управления левой и правой группами ТЭНов
         SideActuators left;
         SideActuators right;
         // Последняя зафиксированная самая высокая Т продукта
         float lastFishHot_C = 0.0f;
         // Флаг, указывающий на наличие предыдущего измерения температуры самой горячей точки продукта
         uint8_t haveLastFishHot = 0;
         // Бит0 = датчик 3, бит1 = датчик 4; смена состава сбрасывает rate_Cps.
         uint8_t prevFishEnableMask = 0;
         // Выпадение зонда из продукта (рост T): авария, канал выключен из алгоритма до возврата в продукт.
         uint8_t productFallen[2] = {0, 0};
         // После выпадения видели переходную вниз (повторная установка).
         uint8_t productCoolRecover[2] = {0, 0};
     };
 
     static ControllerState g;
 
     static void ResetState()
     {
       // Почему: при (пере)запуске нужны предсказуемые состояния без "хвоста" интегратора/ШИМ-памяти.
       g.runtimeSeconds = 0;
       g.piSupplyCommon = PI{ g_defrostParams.piKp, g_defrostParams.piKi, 0.0f };
       g.leftRightTrimGain = g_defrostParams.leftRightTrimGain;
       g.wDeadband_kgkg = g_defrostParams.wDeadband_kgkg;
       g.injPwm.Reset();              // сброс ШИМ-памяти для форсунки.
       g.injHold.Reset(0);            // сброс времени удержания форсунки.
       g.outOn = 0;                   // флаг отключения вытяжки.
       g.outHold_s = 0;               // время удержания вытяжки.
       g.outDamperState = 0;          // заслонка закрыта.
       g.outDamperTimer_s = 0;        // таймер открытия заслонки.
       g.outFanDelay_s = g_defrostParams.outFanDelay_s;
       g.outFanOn = 0;                // вентилятор вытяжки выключен.
       g.shutdownOutFanRemain_s = 0;  // остаток времени работы вытяжки после остановки алгоритма
       g.startupGateClosing = 0;      // при старте: 1 если нужно закрыть ворота
       g.shutdownActive = 0;          // при остановке: 1 пока выполняется последовательность остановки
       g.shutdownGateOpening = 0;
       g.shutdownGateLiftPulse_s = 0;
       g.shutdownGateFullOpenActive = 0;
       g.shutdownGateOpenReleased = 0;
       g.shutdownGateOpenSawActive = 0;
       g.shutdownGateOpenPhase_s = 0;
       g.shutdownStage = 0;
       g.shutdownFlapOpenWait_s = 0;
       g.startPendingAfterShutdown = 0;
       g.alarmBlinkPhase = 0;
       g.startupActuatorDelay_s = 0;
       g.shutdownActuatorDelay_s = 0;
       g.flapTransitionElapsed_s = 0;
       g.flapDesiredOpenPrev = 0;
       g.flapDesiredOpenPrevValid = 0;
       g.flapAlarm = 0;
       g.stagedVent1LeftOn = 0;
       g.stagedVent2LeftOn = 0;
       g.stagedVent1RightOn = 0;
       g.stagedVent2RightOn = 0;
       g.stagedTen1LeftOn = 0;
       g.stagedTen2LeftOn = 0;
       g.stagedTen1RightOn = 0;
       g.stagedTen2RightOn = 0;
       g.left.ten1.Reset();           // сброс ШИМ-памяти для левого ТЭНа 1.
       g.left.ten2.Reset();           // сброс ШИМ-памяти для левого ТЭНа 2.
       g.left.ten1Hold.Reset(0);      // сброс времени удержания левого ТЭНа 1.
       g.left.ten2Hold.Reset(0);      // сброс времени удержания левого ТЭНа 2.
       g.right.ten1.Reset();          // сброс ШИМ-памяти для правого ТЭНа 1.
       g.right.ten2.Reset();          // сброс ШИМ-памяти для правого ТЭНа 2.
       g.right.ten1Hold.Reset(0);     // сброс времени удержания правого ТЭНа 1.
       g.right.ten2Hold.Reset(0);     // сброс времени удержания правого ТЭНа 2.
       g.lastFishHot_C = 0.0f;        // последняя измеренная температура продукта.
       g.haveLastFishHot = 0;         // флаг отсутствия последней измеренной температуры продукта.
       g.prevFishEnableMask = 0;
       g.productFallen[0] = 0;
       g.productFallen[1] = 0;
       g.productCoolRecover[0] = 0;
       g.productCoolRecover[1] = 0; 
     }
 
    enum class LampModeState : uint8_t
    {
        AutoActive = 0,
        StoppedOrManual = 1
    };

   enum class ShutdownStage : uint8_t
   {
       StopActuatorsAndGatePulse = 0, // останов силовых выходов, затем импульс Open на 3 с
       Ventilation = 1,               // открытие заслонки и продувка
       FullGateOpen = 2               // полное открытие ворот
   };

    static void UpdateProductSensorFallout(void)
    {
        // Выпадение: односторонний рост T зонда (лёд растаял, датчик в воздухе).
        // Возврат в алгоритм: переходная вниз и T близка ко второму рабочему датчику продукта.
        const int8_t idx[2] = { kSensFish1_T, kSensFish2_T };
        const int8_t dir[2] = {
            Sensor::GetProductThermalChaseDir((unsigned char)kSensFish1_T),
            Sensor::GetProductThermalChaseDir((unsigned char)kSensFish2_T)
        };
        const float tC[2] = {
            DeciToC(FilteredSensorT_Deci(kSensFish1_T)),
            DeciToC(FilteredSensorT_Deci(kSensFish2_T))
        };
        const uint8_t usable[2] = {
            (uint8_t)(((Sensor_array[kSensFish1_T].Active == 1) && (Sensor_array[kSensFish1_T].UseInDefrost != 0)) ? 1u : 0u),
            (uint8_t)(((Sensor_array[kSensFish2_T].Active == 1) && (Sensor_array[kSensFish2_T].UseInDefrost != 0)) ? 1u : 0u)
        };

        for (uint8_t i = 0u; i < 2u; ++i)
        {
            if (usable[i] == 0u)
            {
                g.productFallen[i] = 0u;
                g.productCoolRecover[i] = 0u;
                continue;
            }
            if (dir[i] > 0)
            {
                g.productFallen[i] = 1u;
                g.productCoolRecover[i] = 0u;
                continue;
            }
            if (g.productFallen[i] == 0u)
                continue;
            if (dir[i] < 0)
                g.productCoolRecover[i] = 1u;
            if ((g.productCoolRecover[i] == 0u) || (dir[i] != 0))
                continue;

            const uint8_t j = (uint8_t)(1u - i);
            const uint8_t otherStable = ((usable[j] != 0u) &&
                (g.productFallen[j] == 0u) &&
                (dir[j] == 0) &&
                (SensorExcludedByClampAlarm(idx[j]) == false)) ? 1u : 0u;
            if (otherStable != 0u)
            {
                float d = tC[i] - tC[j];
                if (d < 0.0f)
                    d = -d;
                if (d <= kProductReinsertMatch_C)
                {
                    g.productFallen[i] = 0u;
                    g.productCoolRecover[i] = 0u;
                }
            }
            else if (usable[j] == 0u)
            {
                // Второго датчика продукта нет — после переходной вниз возвращаем канал.
                g.productFallen[i] = 0u;
                g.productCoolRecover[i] = 0u;
            }
        }

        // Оба выпали и оба снова в продукте: взаимная близость T — достаточный критерий.
        if ((g.productFallen[0] != 0u) && (g.productFallen[1] != 0u) &&
            (g.productCoolRecover[0] != 0u) && (g.productCoolRecover[1] != 0u) &&
            (dir[0] == 0) && (dir[1] == 0) &&
            (usable[0] != 0u) && (usable[1] != 0u))
        {
            float d = tC[0] - tC[1];
            if (d < 0.0f)
                d = -d;
            if (d <= kProductReinsertMatch_C)
            {
                g.productFallen[0] = 0u;
                g.productFallen[1] = 0u;
                g.productCoolRecover[0] = 0u;
                g.productCoolRecover[1] = 0u;
            }
        }

        Model::Device_AlarmFlags = (uint16_t)(Model::Device_AlarmFlags &
            (uint16_t)~(kProductFallenLeftBit | kProductFallenRightBit));
        if (g.productFallen[0] != 0u)
            Model::Device_AlarmFlags |= kProductFallenLeftBit;
        if (g.productFallen[1] != 0u)
            Model::Device_AlarmFlags |= kProductFallenRightBit;
    }

    static void UpdateDeviceAlarmState()
    {
        // Биты Device_AlarmFlags для аварий ворот:
        // bit9  - программная авария ворот (таймаут движения, нет фронта концевика за 10 с)
        // bit10 - аппаратная авария ворот (вход Gate_Alarm модуля IO)
        // bit11 - авария заслонки вытяжки (нет нужного сигнала Air_Open/Air_Close за DEFROST_FLAP_POSITION_TIMEOUT_S с)
        // bit13 - выпадение датчика продукта 3 (зонд в воздухе)
        // bit14 - выпадение датчика продукта 4
        // Биты 0..8 - аварии рассогласования выход/вход (в т.ч. _Out) из проверки DeviceSwitchCheck.
        const uint16_t kGateProgramAlarmBit = (uint16_t)(1u << 9);
        const uint16_t kGateHardwareAlarmBit = (uint16_t)(1u << 10);
        const uint16_t kFlapAlarmBit = (uint16_t)(1u << 11);
        const uint16_t kDeviceSwitchCheckMask = (uint16_t)((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
                                                            (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8));
        const bool deviceSwitchCheckEnabled = (g_defrostParams.debugDisableDeviceSwitchCheck == 0u);

        if (deviceSwitchCheckEnabled)
        {
            const uint8_t desiredOpen = ((Model::Flag_DFR_manual != 0u) ? Model::DFR_manual.Water_Flap : Model::DFR.Water_Flap) ? 1u : 0u;
            const uint8_t airOpen = (Model::DI_DFR.Bits.Air_Open != 0u) ? 1u : 0u;
            const uint8_t airClose = (Model::DI_DFR.Bits.Air_Close != 0u) ? 1u : 0u;
            const uint8_t flapReached = desiredOpen ? ((airOpen != 0u && airClose == 0u) ? 1u : 0u)
                                                    : ((airClose != 0u && airOpen == 0u) ? 1u : 0u);
            if (g.flapDesiredOpenPrevValid == 0u || g.flapDesiredOpenPrev != desiredOpen)
            {
                g.flapDesiredOpenPrev = desiredOpen;
                g.flapDesiredOpenPrevValid = 1u;
                g.flapTransitionElapsed_s = 0u;
                g.flapAlarm = 0u;
            }
            if (flapReached != 0u)
            {
                g.flapTransitionElapsed_s = 0u;
                g.flapAlarm = 0u;
            }
            else
            {
                if (g.flapTransitionElapsed_s < DEFROST_FLAP_POSITION_TIMEOUT_S)
                {
                    ++g.flapTransitionElapsed_s;
                }
                if (g.flapTransitionElapsed_s >= DEFROST_FLAP_POSITION_TIMEOUT_S)
                {
                    g.flapAlarm = 1u;
                }
            }
        }
        else
        {
            // При отключённой проверке не формируем аварии _Out/заслонки и очищаем их.
            g.flapTransitionElapsed_s = 0u;
            g.flapDesiredOpenPrevValid = 0u;
            g.flapAlarm = 0u;
            Model::Device_AlarmFlags = (uint16_t)(Model::Device_AlarmFlags & (uint16_t)~kDeviceSwitchCheckMask);
        }

        Model::Device_AlarmFlags = (uint16_t)(Model::Device_AlarmFlags & (uint16_t)~(kGateProgramAlarmBit | kGateHardwareAlarmBit | kFlapAlarmBit));
        if (deviceSwitchCheckEnabled && Model::Gate_Alarm_Program != 0u)
        {
            Model::Device_AlarmFlags |= kGateProgramAlarmBit;
        }
        if (Model::Gate_Alarm_Hardware != 0u)
        {
            Model::Device_AlarmFlags |= kGateHardwareAlarmBit;
        }
        if (deviceSwitchCheckEnabled && g.flapAlarm != 0u)
        {
            Model::Device_AlarmFlags |= kFlapAlarmBit;
        }

        // Общий флаг аварии устройства: авария ворот ИЛИ аварии из регистра Device_AlarmFlags.
        Model::Device_Alarm = ((GateControl_IsAlarm() != 0) || (Model::Device_AlarmFlags != 0) || (Model::Sensor_AlarmFlags != 0)) ? 1 : 0;

        if (Model::Device_Alarm != 0)
        {
            // Мигаем 1 сек вкл / 1 сек выкл (тик DefrostControl_Update1s = 1 Гц).
            g.alarmBlinkPhase = (g.alarmBlinkPhase == 0) ? 1 : 0;
        }
        else
        {
            g.alarmBlinkPhase = 0;
        }
    }

    // Управление лампами индикации:
    // зелёная _Wrk - только активный автоматический режим;
    // красная _Alr - только авария устройства, мигание 1/1 сек.
    static void ApplyModeLamps(LampModeState modeState)
    {
        // В ручном режиме никакие лампы не горят.
        if (Model::Flag_DFR_manual != 0)
        {
            Model::DFR._Wrk = 0;
            Model::DFR._Alr = 0;
            Model::DFR_manual._Wrk = 0;
            Model::DFR_manual._Alr = 0;
            return;
        }

        Model::DFR._Wrk = (modeState == LampModeState::AutoActive) ? 1 : 0;
        Model::DFR._Alr = (Model::Device_Alarm != 0 && g.alarmBlinkPhase != 0) ? 1 : 0;
        Model::DFR_manual._Wrk = 0;
        Model::DFR_manual._Alr = 0;
    }

    // Это “диспетчер” задания состояний исполнительных механизмов на основе желаемых значений
    // Соблюдение порядка последовательного, с интервалами времени, включения и выключения мощных устройств
    static void ApplyOutputs(
         uint8_t ventLeftOn,
         uint8_t ventRightOn,
         uint8_t ten1LeftOn,
         uint8_t ten2LeftOn,
         uint8_t ten1RightOn,
         uint8_t ten2RightOn,
         uint8_t injOn,
         uint8_t outOn)
     {
        // Почему: чтобы снизить пусковые токи, включаем вентиляторы и ТЭНы по одному каналу
        // с интервалом 2 секунды между включениями, а при выключении гасим в обратном порядке:
        // сначала ТЭНы, затем вентиляторы.
        const uint8_t kStartupInterval_s = 2;
        if (g.startupActuatorDelay_s > 0)
        {
            g.startupActuatorDelay_s--;
        }
        if (g.shutdownActuatorDelay_s > 0)
        {
            g.shutdownActuatorDelay_s--;
        }

        bool switchedOnThisTick = false;
        bool switchedOffThisTick = false;

        auto stageOn = [&](uint8_t desiredOn, uint8_t& stagedOn)
        {
            if (desiredOn == 0) // Алгоритм хочет выключить устройство
            {
                return;
            }
            if (stagedOn != 0)  // Устройство уже включено
            {
                return;
            }
            // Если устройство ещё не было включено за эту секунду, и время задержки равно 0,
            // то включаем устройство.
            if (!switchedOnThisTick && g.startupActuatorDelay_s == 0)
            {
                stagedOn = 1;
                switchedOnThisTick = true;
                g.startupActuatorDelay_s = kStartupInterval_s;
            }
        };

        auto stageOff = [&](uint8_t desiredOn, uint8_t& stagedOn)
        {
            if (desiredOn != 0) // Алгоритм хочет держать устройство включенным
            {
                return;
            }
            if (stagedOn == 0)  // Устройство уже выключено
            {
                return;
            }
            // Выключаем по одному каналу за тик.
            if (!switchedOffThisTick && g.shutdownActuatorDelay_s == 0)
            {
                stagedOn = 0;
                switchedOffThisTick = true;
                g.shutdownActuatorDelay_s = kStartupInterval_s;
            }
        };

        // Вентиляторы запускаем раньше ТЭНов.
        stageOn(ventLeftOn,  g.stagedVent1LeftOn);
        stageOn(ventLeftOn,  g.stagedVent2LeftOn);
        stageOn(ventRightOn, g.stagedVent1RightOn);
        stageOn(ventRightOn, g.stagedVent2RightOn);
        stageOn(ten1LeftOn,  g.stagedTen1LeftOn);
        stageOn(ten2LeftOn,  g.stagedTen2LeftOn);
        stageOn(ten1RightOn, g.stagedTen1RightOn);
        stageOn(ten2RightOn, g.stagedTen2RightOn);

        // Выключение делаем в обратном порядке: сначала ТЭНы, затем вентиляторы.
        stageOff(ten2RightOn, g.stagedTen2RightOn);
        stageOff(ten1RightOn, g.stagedTen1RightOn);
        stageOff(ten2LeftOn,  g.stagedTen2LeftOn);
        stageOff(ten1LeftOn,  g.stagedTen1LeftOn);
        stageOff(ventRightOn, g.stagedVent2RightOn);
        stageOff(ventRightOn, g.stagedVent1RightOn);
        stageOff(ventLeftOn,  g.stagedVent2LeftOn);
        stageOff(ventLeftOn,  g.stagedVent1LeftOn);

        // Загружаем значения в регистр управления устройствами
        Model::DFR.Vent1_Left  = g.stagedVent1LeftOn ? 1 : 0;
        Model::DFR.Vent2_Left  = g.stagedVent2LeftOn ? 1 : 0;
        Model::DFR.Vent1_Right = g.stagedVent1RightOn ? 1 : 0;
        Model::DFR.Vent2_Right = g.stagedVent2RightOn ? 1 : 0;

        Model::DFR.Ten1_Left   = g.stagedTen1LeftOn ? 1 : 0;
        Model::DFR.Ten2_Left   = g.stagedTen2LeftOn ? 1 : 0;
        Model::DFR.Ten1_Right  = g.stagedTen1RightOn ? 1 : 0;
        Model::DFR.Ten2_Right  = g.stagedTen2RightOn ? 1 : 0;
 
         Model::DFR._Inj        = injOn ? 1 : 0;
         Model::DFR._Out        = outOn ? 1 : 0;
        ApplyModeLamps(LampModeState::AutoActive);
     }

     // Вытяжка по ошибке влажности: сначала команда Water_Flap, вентилятор (_Out) — только после открытия заслонки.
     static void StepExhaustByHumidityError(float wErr)
     {
         if (g.outHold_s > 0u)
             g.outHold_s--;

         if (g.outHold_s == 0u)
         {
             if (wErr < -g.wDeadband_kgkg)
             {
                 if (g.outDamperState == 0u)
                 {
                     g.outDamperState = 1u;
                     g.outDamperTimer_s = g_defrostParams.outDamperTimer_s;
                     g.outFanOn = 0u;
                     g.outHold_s = g_defrostParams.outHold_s;
                 }
             }
             else if (wErr > 0.0f)
             {
                 g.outDamperState = 0u;
                 g.outDamperTimer_s = 0u;
                 g.outFanOn = 0u;
                 g.outHold_s = g_defrostParams.outHold_s;
             }
         }

         if (g.outDamperState != 0u)
             Model::DFR.Water_Flap = 1u;
         else
             Model::DFR.Water_Flap = 0u;

         const uint8_t flapOpened =
             ((Model::DI_DFR.Bits.Air_Open != 0u) && (Model::DI_DFR.Bits.Air_Close == 0u)) ? 1u : 0u;

         if (g.outDamperState == 1u)
         {
             g.outFanOn = 0u;
             if (flapOpened != 0u)
             {
                 g.outDamperState = 2u;
                 g.outDamperTimer_s = g_defrostParams.outFanDelay_s;
             }
             else if (g.outDamperTimer_s > 0u)
             {
                 g.outDamperTimer_s--;
             }
             else if (DefrostControl_IsDeviceSwitchCheckEnabled() == 0u)
             {
                 // Концевики не используем: после таймера считаем заслонку открытой.
                 g.outDamperState = 2u;
                 g.outDamperTimer_s = g_defrostParams.outFanDelay_s;
             }
         }
         else if (g.outDamperState == 2u)
         {
             if (g.outDamperTimer_s > 0u)
                 g.outDamperTimer_s--;
             else
             {
                 g.outDamperState = 3u;
                 g.outFanOn = 1u;
             }
         }

         g.outOn = g.outFanOn;
     }

     static void StopExhaustHumidity(void)
     {
         g.outDamperState = 0u;
         g.outDamperTimer_s = 0u;
         g.outFanOn = 0u;
         g.outOn = 0u;
         g.outHold_s = 0u;
         Model::DFR.Water_Flap = 0u;
     }
 
    static void ControlStep1s()
     {
       // Water_Flap в авто-режиме открывается только когда контур влажности включает вытяжку.

        if (g.startupGateClosing != 0)
        {
            // Закрытие ворот выполняется через API GateControl:
            // там учитываются фронты концевика и аварийный тайм-аут 10 секунд.
            if (GateControl_IsCommandActive(GateControlCommand::Close) == 0)
            {
                g.startupGateClosing = 0;
            }
            else
            {
                return;  // выходы регистра DFR уже обнулены при старте закрытия ворот
            }
        }

       // В автоматическом режиме воротами управляет только GateControl.
       // Инициализация битов ворот выполняется при входе в авто-режим и при завершении команд; здесь не сбрасываем.

       /**********************************
       СОБСТВЕННО АЛГОРИТМ АВТОМАТИЧЕСКОГО УПРАВЛЕНИЯ
       ************************************/  
       // Воздушный контур: T/RH подачи и возврата с учётом отключённых датчиков (Active) и аварии обрезок.
         DefrostAirControlInputs airIn = {};
         ComputeDefrostAirControlInputs(&airIn);    // получаем значения с датчиков воздушного потока
         const float RH_ret   = DeciToRH((int16_t)Model::getCurrentVal_H(kSensReturn_T_H));
         (void)RH_ret;

         const float fish1_C = DeciToC(FilteredSensorT_Deci(kSensFish1_T));
         const float fish2_C = DeciToC(FilteredSensorT_Deci(kSensFish2_T));

         // Почему: проверяем, что датчик активен, используется в дефросте и не в тепловой догонке/аварии обрезок.
         const bool fish1Enabled = (Sensor_array[kSensFish1_T].Active == 1) && (Sensor_array[kSensFish1_T].UseInDefrost != 0)
             && !SensorExcludedByClampAlarm(kSensFish1_T)
             && (g.productFallen[0] == 0u)
             && (Sensor::IsProductThermalTransient((unsigned char)kSensFish1_T) == 0u);
         const bool fish2Enabled = (Sensor_array[kSensFish2_T].Active == 1) && (Sensor_array[kSensFish2_T].UseInDefrost != 0)
             && !SensorExcludedByClampAlarm(kSensFish2_T)
             && (g.productFallen[1] == 0u)
             && (Sensor::IsProductThermalTransient((unsigned char)kSensFish2_T) == 0u);

         const uint8_t fishEnableMask = (uint8_t)((fish1Enabled ? 1u : 0u) | (fish2Enabled ? 2u : 0u));
         if (fishEnableMask != g.prevFishEnableMask)
         {
             // Смена набора рабочих датчиков продукта: не считать rate_Cps от несравнимых T.
             g.haveLastFishHot = 0;
             g.prevFishEnableMask = fishEnableMask;
         }

         float fishHot_C = 0.0f;
         float fishCold_C = 0.0f;
         float fishDelta_C = 0.0f;

         // Установка параметров алгоритма при наличии двух датчиков продукта
         if (fish1Enabled && fish2Enabled)
         {
             fishHot_C = (fish1_C >= fish2_C) ? fish1_C : fish2_C;
             fishCold_C = (fish1_C < fish2_C) ? fish1_C : fish2_C;
             fishDelta_C = fishHot_C - fishCold_C;
         }
         // Установка параметров алгоритма при наличии только первого датчика продукта
         else if (fish1Enabled)
         {
             fishHot_C = fish1_C;
             fishCold_C = fish1_C;
             fishDelta_C = 0.0f;
         }
         // Установка параметров алгоритма при наличии только второго датчика продукта
         else if (fish2Enabled)
         {
             fishHot_C = fish2_C;
             fishCold_C = fish2_C;
             fishDelta_C = 0.0f;
         }
        // Без датчиков продукта — режим «только по воздуху» (фаза дефроста определяется по времени, лимит T подачи).
        else
        {
            g.haveLastFishHot = 0;
            ControlStep1s_AirOnly();
            return;
        }

        // Останов алгоритма при достижении целевой мин. температуры рыбы (целевая Т задаётся в параметрах/на Settings1).
        // В отладке автостоп можно отключить параметром debugDisableTargetTStop.
        if ((g_defrostParams.debugDisableTargetTStop == 0u) &&
            (fishCold_C >= g_defrostParams.fishColdTarget_C))
        {
            DefrostControl_SetEnabled(0);
            return;
        }

         // Почему: выбираем фазу разморозки на основе температуры самой холодной точки продукта.
         const Phase phase = SelectPhase(fishCold_C);
         const Limits lim = GetLimits(phase);
         const Targets tgt = GetTargets(phase);
 
         // Переводим уставку влажности в абсолютную влажность. Текущая влажность — по исправным каналам подачи.
         const float T_sup_avg_C = airIn.T_sup_avg_C;
         const float w_sup_avg   = HumidityRatio_kgkg(T_sup_avg_C, airIn.RH_sup_avg);
         const float w_ret_target = HumidityRatio_kgkg(airIn.T_ret_C, tgt.returnTargetRH_percent);
 
         // ─────────────────────────────────────────────────────────────────────────
         // Ограничитель безопасности: уменьшаем нагрев при приближении к ограничениям качества продукта.
         // Почему: ТЭНы мощные, продукт инерционный — проактивное ограничение предотвращает "перелёт".
         // ─────────────────────────────────────────────────────────────────────────
         // Переменная heatScale01 используется как ограничитель мощности нагрева для защиты качества продукта. 
         // Она представляет собой коэффициент (от 0.0 до 1.0), который умножается на расчётную мощность ТЭНов.
         float heatScale01 = 1.0f;

         // Жёсткий потолок температуры "горячей" точки продукта.
         if (fishHot_C >= lim.fishHotMax_C)
         {
             heatScale01 = 0.0f;
         }
         else
         {
             // Мягкое снижение мощности в зоне ниже лимита.
             // Если осталось менее 0.6C до лимита, то снижаем мощность нагрева.
             const float margin_C = 0.6f;
             const float x = (lim.fishHotMax_C - fishHot_C) / margin_C; // коэффициент удалённости от лимита
             heatScale01 = Clamp(x, 0.0f, 1.0f); // ограничиваем значение от 0 до 1
         }
 
         // Ограничение по скорости прогрева "горячей" точки (только при наличии предыдущей температуры продукта).
         float rate_Cps = 0.0f;   // для лога; при отсутствии предыдущего замера — 0
         if (g.haveLastFishHot != 0)
         {
             rate_Cps = (fishHot_C - g.lastFishHot_C) / kDt_s; // скорость прогрева "горячей" точки, град/сек
             if (rate_Cps > lim.fishHotRateMax_Cps) // если скорость прогрева превышает максимально допустимую
             {
                 // Пропорционально снижаем; защищаемся от полного "затыка" из-за шума.
                const float s = lim.fishHotRateMax_Cps / Clamp(rate_Cps, 0.01f, rate_Cps); // коэффициент снижения мощности нагрева
                heatScale01 = Clamp(heatScale01 * s, 0.0f, 1.0f); // ограничиваем значение от 0 до 1
             }
         }
         g.lastFishHot_C = fishHot_C;
         g.haveLastFishHot = 1;
 
         // Ограничение по неравномерности прогрева (Т поверхности - Т сердцевины).
         if (fishDelta_C > lim.fishDeltaMax_C)
         {
             // Почему: большой градиент означает, что поверхность греется быстрее сердцевины.
             heatScale01 = 0.0f; // отключаем нагрев
         }
 
         // Ограничение по температуре подачи — по исправным датчикам подачи или по оценке T_подачи = T_возврата + 2 °C.
         if ((airIn.okL != 0u && airIn.mL > lim.supplyMax_C) || (airIn.okR != 0u && airIn.mR > lim.supplyMax_C))
         {
             heatScale01 = 0.0f;
         }
         if (airIn.okL == 0u && airIn.okR == 0u && airIn.okRet != 0u
             && (airIn.mRet + kSupplyTInferOffsetFromReturn_C) > lim.supplyMax_C)
         {
             heatScale01 = 0.0f;
         }
         // Полный отказ каналов 0..2: не вести ПИ по воздуху.
         if (airIn.okL == 0u && airIn.okR == 0u && airIn.okRet == 0u)
         {
             heatScale01 = 0.0f;
         }
 
         // ─────────────────────────────────────────────────────────────────────────
         // Регулирование температуры (общая составляющая + баланс лево/право).
         // Выход выражен в "эквивалентах ТЭНа": 0..2 на сторону (два ТЭНа на сторону).
         // Почему: так отображение на актуаторы явно и удобно строить скважность.
         // ─────────────────────────────────────────────────────────────────────────
         const float eT_common = tgt.supplySet_C - T_sup_avg_C;
 
         // Базовый запрос мощности с учётом ограничителя.
         float uCommon_TEN = g.piSupplyCommon.Step(eT_common, kDt_s, 0.0f, 2.0f);
         uCommon_TEN *= heatScale01;
 
         // Балансировка лево/право только если обе подачи исправны; иначе симметричное управление (без перекоса по «нулю»).
         const float eT_diff = (airIn.okL != 0u && airIn.okR != 0u) ? (airIn.mL - airIn.mR) : 0.0f;
        float trim_TEN = g.leftRightTrimGain * eT_diff;
        trim_TEN = Clamp(trim_TEN, -g_defrostParams.leftRightTrimMaxEq, g_defrostParams.leftRightTrimMaxEq);
 
         float uLeft_TEN  = Clamp(uCommon_TEN - trim_TEN, 0.0f, 2.0f);
         float uRight_TEN = Clamp(uCommon_TEN + trim_TEN, 0.0f, 2.0f);
 
         // Преобразуем "эквиваленты ТЭНа" в скважности для каждого ТЭНа.
         const float leftTen1Duty  = Clamp(uLeft_TEN, 0.0f, 1.0f);
         const float leftTen2Duty  = Clamp(uLeft_TEN - 1.0f, 0.0f, 1.0f);
         const float rightTen1Duty = Clamp(uRight_TEN, 0.0f, 1.0f);
         const float rightTen2Duty = Clamp(uRight_TEN - 1.0f, 0.0f, 1.0f);
 
         const uint8_t ten1L_desired = g.left.ten1.Step(leftTen1Duty);
         const uint8_t ten2L_desired = g.left.ten2.Step(leftTen2Duty);
         const uint8_t ten1R_desired = g.right.ten1.Step(rightTen1Duty);
         const uint8_t ten2R_desired = g.right.ten2.Step(rightTen2Duty);
 
         // Почему: защита от слишком частого щёлканья реле ТЭНов.
         //         Значение minHold нужно подбирать под силовую часть (реле/контактор/SSR).
        // Ниже 10 с удержание не защищает реле от частых переключений; при сбое параметра задаём минимум 10 с.
        const uint16_t kTenMinHold_s = (g_defrostParams.tenMinHold_s >= 10u) ? g_defrostParams.tenMinHold_s : 10u;
         const uint8_t ten1L_on = g.left.ten1Hold.Step(ten1L_desired, kTenMinHold_s);
         const uint8_t ten2L_on = g.left.ten2Hold.Step(ten2L_desired, kTenMinHold_s);
         const uint8_t ten1R_on = g.right.ten1Hold.Step(ten1R_desired, kTenMinHold_s);
         const uint8_t ten2R_on = g.right.ten2Hold.Step(ten2R_desired, kTenMinHold_s);
 
         // Вентиляторы: работаем непрерывно, пока алгоритм включен.
         // Почему: стабильный расход упрощает идентификацию контуров и защищает ТЭНы от локального перегрева.
         const uint8_t ventL_on = 1;
         const uint8_t ventR_on = 1;
 
         // ─────────────────────────────────────────────────────────────────────────
         // Управление влажностью (форсунка + вытяжка) по абсолютной влажности: среднее по подаче лево/право.
         // Почему: абсолютная влажность лучше отражает "сушащее" действие воздуха, чем RH (RH зависит от T).
         // ─────────────────────────────────────────────────────────────────────────
         const float wErr = w_ret_target - w_sup_avg;
 
         float injDuty = 0.0f;
         if (airIn.humidityMeasOk != 0u)
         {
             if (wErr > g.wDeadband_kgkg)
             {
                 injDuty = Clamp(g_defrostParams.injGain * (wErr - g.wDeadband_kgkg), 0.0f, 1.0f);
             }
             StepExhaustByHumidityError(wErr);
             if (g.outOn != 0)
                 injDuty = 0.0f;
         }
         else
         {
             // Нет T/RH воздуха 0..2: не гонять форсунку и вытяжку по заглушкам T=0 / RH=50%.
             StopExhaustHumidity();
         }
 
         const uint8_t inj_desired = g.injPwm.Step(injDuty);
         // Почему: форсунка (клапан/насос) тоже не любит слишком частые переключения.
        const uint16_t kInjMinHold_s = g_defrostParams.injMinHold_s;
         const uint8_t inj_on = g.injHold.Step(inj_desired, kInjMinHold_s);
         const uint8_t out_on = g.outOn ? 1 : 0;

        s_lastPhase = static_cast<uint8_t>(phase);

        // Формируем текущее состояние алгоритма управления для регулярного лога (6 датчиков: 0..5).
        s_controlLogPayload.T_filt_C[0] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 0, 4));  // defroster T,H left
        s_controlLogPayload.T_filt_C[1] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 1, 4));  // defroster T,H right
        s_controlLogPayload.T_filt_C[2] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 2, 4));  // defroster T,H center
        s_controlLogPayload.T_filt_C[3] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 3, 4));  // fish T left
        s_controlLogPayload.T_filt_C[4] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 4, 4));  // fish T right
        s_controlLogPayload.T_filt_C[5] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 5, 4));  // defroster operating T

         s_controlLogPayload.phase = s_lastPhase;
         s_controlLogPayload.eT_common = eT_common;
         s_controlLogPayload.heatScale01 = heatScale01;
         s_controlLogPayload.uCommon_TEN = uCommon_TEN;
         s_controlLogPayload.trim_TEN = trim_TEN;
         s_controlLogPayload.uLeft_TEN = uLeft_TEN;
         s_controlLogPayload.uRight_TEN = uRight_TEN;
         s_controlLogPayload.leftTen1Duty = leftTen1Duty;
         s_controlLogPayload.leftTen2Duty = leftTen2Duty;
         s_controlLogPayload.rightTen1Duty = rightTen1Duty;
         s_controlLogPayload.rightTen2Duty = rightTen2Duty;
         s_controlLogPayload.w_sup_avg = w_sup_avg;
         s_controlLogPayload.wErr = wErr;
         s_controlLogPayload.injDuty = injDuty;
         s_controlLogPayload.rate_Cps = rate_Cps;
         s_controlLogPayload.fishHot_C = fishHot_C;
         s_controlLogPayload.fishCold_C = fishCold_C;

         // Устанавливаем порядок включения мощных устройств с интервалом между включениями
         ApplyOutputs(
             /*ventLeftOn*/  ventL_on,
             /*ventRightOn*/ ventR_on,
             /*ten1LeftOn*/  ten1L_on,
             /*ten2LeftOn*/  ten2L_on,
             /*ten1RightOn*/ ten1R_on,
             /*ten2RightOn*/ ten2R_on,
             /*injOn*/       inj_on,
             /*outOn*/       out_on);
     }

     // Управление без датчиков продукта: только по T/RH подачи и возврата, фаза по времени, лимит T подачи и макс. время.
     static void ControlStep1s_AirOnly()
     {
         DefrostAirControlInputs airIn = {};
         ComputeDefrostAirControlInputs(&airIn);
         const float RH_ret     = DeciToRH((int16_t)Model::getCurrentVal_H(kSensReturn_T_H));
         (void)RH_ret;

         const float T_sup_avg_C = airIn.T_sup_avg_C;
         const float w_sup_avg  = HumidityRatio_kgkg(T_sup_avg_C, airIn.RH_sup_avg);

         const Phase phase = SelectPhaseByTime(g.runtimeSeconds);
         const Limits lim  = GetLimits(phase);
         const Targets tgt = GetTargets(phase);
         const float w_ret_target = HumidityRatio_kgkg(airIn.T_ret_C, tgt.returnTargetRH_percent);

         float heatScale01 = 1.0f;
         if ((airIn.okL != 0u && airIn.mL > lim.supplyMax_C) || (airIn.okR != 0u && airIn.mR > lim.supplyMax_C))
             heatScale01 = 0.0f;
         if (airIn.okL == 0u && airIn.okR == 0u && airIn.okRet != 0u
             && (airIn.mRet + kSupplyTInferOffsetFromReturn_C) > lim.supplyMax_C)
             heatScale01 = 0.0f;
         if (airIn.okL == 0u && airIn.okR == 0u && airIn.okRet == 0u)
             heatScale01 = 0.0f;
        if (g.runtimeSeconds >= (uint32_t)g_defrostParams.maxRuntime_s)
        {
            // Без рабочих датчиков продукта завершаем алгоритм по общему лимиту времени.
            // Дальнейшая последовательность остановки выполняется в ShutdownSequence().
            DefrostControl_SetEnabled(0);
            return;
        }

         const float eT_common = tgt.supplySet_C - T_sup_avg_C;
         float uCommon_TEN = g.piSupplyCommon.Step(eT_common, kDt_s, 0.0f, 2.0f);
         uCommon_TEN *= heatScale01;

         const float eT_diff = (airIn.okL != 0u && airIn.okR != 0u) ? (airIn.mL - airIn.mR) : 0.0f;
         float trim_TEN = g.leftRightTrimGain * eT_diff;
         trim_TEN = Clamp(trim_TEN, -g_defrostParams.leftRightTrimMaxEq, g_defrostParams.leftRightTrimMaxEq);

         float uLeft_TEN  = Clamp(uCommon_TEN - trim_TEN, 0.0f, 2.0f);
         float uRight_TEN = Clamp(uCommon_TEN + trim_TEN, 0.0f, 2.0f);

         const float leftTen1Duty  = Clamp(uLeft_TEN, 0.0f, 1.0f);
         const float leftTen2Duty  = Clamp(uLeft_TEN - 1.0f, 0.0f, 1.0f);
         const float rightTen1Duty = Clamp(uRight_TEN, 0.0f, 1.0f);
         const float rightTen2Duty = Clamp(uRight_TEN - 1.0f, 0.0f, 1.0f);

         const uint8_t ten1L_desired = g.left.ten1.Step(leftTen1Duty);
         const uint8_t ten2L_desired = g.left.ten2.Step(leftTen2Duty);
         const uint8_t ten1R_desired = g.right.ten1.Step(rightTen1Duty);
         const uint8_t ten2R_desired = g.right.ten2.Step(rightTen2Duty);

         // Ниже 10 с удержание не защищает реле от частых переключений; при сбое параметра задаём минимум 10 с.
         const uint16_t kTenMinHold_s = (g_defrostParams.tenMinHold_s >= 10u) ? g_defrostParams.tenMinHold_s : 10u;
         const uint8_t ten1L_on = g.left.ten1Hold.Step(ten1L_desired, kTenMinHold_s);
         const uint8_t ten2L_on = g.left.ten2Hold.Step(ten2L_desired, kTenMinHold_s);
         const uint8_t ten1R_on = g.right.ten1Hold.Step(ten1R_desired, kTenMinHold_s);
         const uint8_t ten2R_on = g.right.ten2Hold.Step(ten2R_desired, kTenMinHold_s);

         const uint8_t ventL_on = 1;
         const uint8_t ventR_on = 1;

         const float wErr = w_ret_target - w_sup_avg;
         float injDuty = 0.0f;
         if (airIn.humidityMeasOk != 0u)
         {
             if (wErr > g.wDeadband_kgkg)
                 injDuty = Clamp(g_defrostParams.injGain * (wErr - g.wDeadband_kgkg), 0.0f, 1.0f);
             StepExhaustByHumidityError(wErr);
             if (g.outOn != 0)
                 injDuty = 0.0f;
         }
         else
         {
             StopExhaustHumidity();
         }

         const uint8_t inj_desired = g.injPwm.Step(injDuty);
         const uint16_t kInjMinHold_s = g_defrostParams.injMinHold_s;
         const uint8_t inj_on = g.injHold.Step(inj_desired, kInjMinHold_s);
         const uint8_t out_on = g.outOn ? 1 : 0;

        s_lastPhase = static_cast<uint8_t>(phase);
        s_controlLogPayload.phase = s_lastPhase;
        s_controlLogPayload.eT_common = eT_common;
        s_controlLogPayload.heatScale01 = heatScale01;
        s_controlLogPayload.uCommon_TEN = uCommon_TEN;
        s_controlLogPayload.trim_TEN = trim_TEN;
        s_controlLogPayload.uLeft_TEN = uLeft_TEN;
        s_controlLogPayload.uRight_TEN = uRight_TEN;
        s_controlLogPayload.leftTen1Duty = leftTen1Duty;
        s_controlLogPayload.leftTen2Duty = leftTen2Duty;
        s_controlLogPayload.rightTen1Duty = rightTen1Duty;
        s_controlLogPayload.rightTen2Duty = rightTen2Duty;

        // Отфильтрованные температуры 6 датчиков (индексы 0..5, в °C) — первыми в логе.
        s_controlLogPayload.T_filt_C[0] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 0, 4));  // defroster T,H left
        s_controlLogPayload.T_filt_C[1] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 1, 4));  // defroster T,H right
        s_controlLogPayload.T_filt_C[2] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 2, 4));  // defroster T,H center
        s_controlLogPayload.T_filt_C[3] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 3, 4));  // fish T left
        s_controlLogPayload.T_filt_C[4] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 4, 4));  // fish T right
        s_controlLogPayload.T_filt_C[5] = DeciToC((int16_t)Sensor::GetData(TimeFromStart, 5, 4));  // defroster operating T

        s_controlLogPayload.w_sup_avg = w_sup_avg;
        s_controlLogPayload.wErr = wErr;
        s_controlLogPayload.injDuty = injDuty;
        s_controlLogPayload.rate_Cps = 0.0f;
        s_controlLogPayload.fishHot_C = 0.0f;
        s_controlLogPayload.fishCold_C = 0.0f;

         ApplyOutputs(
             ventL_on, ventR_on,
             ten1L_on, ten2L_on, ten1R_on, ten2R_on,
             inj_on, out_on);
     }

// Бит рассогласования DO/DI вентилятора вытяжки (Vent_Out); совпадает с битом 8 в kDeviceCheckMask (ModBus.cpp).
static constexpr uint16_t kDeviceAlarmVentOutBit = (uint16_t)(1u << 8);
// Если команда Open на ворота так и не стала «активной» в GateControl — снять её через N с (защита от зависания).
static constexpr uint8_t kShutdownGateOpenStuckTimeout_s = 15u;

// Переход в шаг полного открытия ворот + немедленная команда Open.
// Почему сразу: при переходе из Ventilation в этом же вызове ProcessShutdownStage1s ветка «шаг 3» не выполняется;
// если только выставить shutdownStage, до следующей секунды shutdownGateFullOpenActive остаётся 0 и
// DefrostControl_Update1s ошибочно считает этап завершённым (сбрасывает shutdownActive без _Opn/Gate_Up).
static void ShutdownEnterFullGateOpenArm()
{
    g.shutdownStage = (uint8_t)ShutdownStage::FullGateOpen;
    g.shutdownGateOpenReleased = 0u;
    g.shutdownGateOpenSawActive = 0u;
    g.shutdownGateOpenPhase_s = 0u;
    g.shutdownFlapOpenWait_s = 0u;
    if (g.shutdownGateFullOpenActive == 0u) {
        GateControl_SetCommand(GateControlCommand::Open, 1u);
        g.shutdownGateFullOpenActive = 1u;
    }
}

static void ShutdownGoToFullGateOpen_VentCleanup()
{
    g.outFanOn = 0u;
    g.shutdownOutFanRemain_s = 0u;
    g.shutdownFlapOpenWait_s = 0u;
    Model::DFR._Out = 0u;
    Model::DFR.Water_Flap = 0u;
    ShutdownEnterFullGateOpenArm();
}
 
 static void ShutdownSequence()
{
    // Безопасное выключение всех исполнительных механизмов при остановке алгоритма

    // Форсунки: отключить увлажнение
    Model::DFR._Inj = 0;

    // Подготавливаем состояния для завершения post-shutdown.
    g.shutdownStage = (uint8_t)ShutdownStage::StopActuatorsAndGatePulse;
    g.shutdownFlapOpenWait_s = 0;
    g.shutdownGateOpening = 0;
    g.shutdownGateLiftPulse_s = 0u;
    g.shutdownGateFullOpenActive = 0;
    g.shutdownGateOpenReleased = 0;
    g.shutdownGateOpenSawActive = 0;
    g.shutdownGateOpenPhase_s = 0;
    // Открываем заслонку сразу с началом завершения процесса.
    // Вытяжка (_Out) остаётся выключенной до шага Ventilation.
    Model::DFR.Water_Flap = 1u;
    g.outDamperState = 0;
    g.outDamperTimer_s = 0;
    g.outFanOn = 0;
    g.outOn = 0;
    Model::DFR._Out = 0;
    g.shutdownOutFanRemain_s = 0u;   // таймер продувки стартует в Ventilation после проверки Vent_Out
    g.shutdownActive = 1;   // флаг завершения алгоритма (сигнал _Shd на сервере)

    // Запускаем последовательное выключение ТЭНов/вентиляторов через ApplyOutputs().
    // ТЭНы гасим первыми, вентиляторы последними.
    g.shutdownActuatorDelay_s = 0;
    ApplyOutputs(0, 0, 0, 0, 0, 0, 0, 0);
}

static void ProcessShutdownStage1s()
{
    // Последовательное выключение энергоёмких устройств:
    // сначала ТЭНы, затем вентиляторы (в обратном порядке включению).
    // Вытяжку (_Out) и форсунку (_Inj) не трогаем этой очередью (они управляются отдельно).
    ApplyOutputs(   // выключаем все ТЭНы и вентиляторы
        /*ventLeftOn*/ 0,
        /*ventRightOn*/ 0,
        /*ten1LeftOn*/ 0,
        /*ten2LeftOn*/ 0,
        /*ten1RightOn*/ 0,
        /*ten2RightOn*/ 0,
        /*injOn*/ 0,
        /*outOn*/ (Model::DFR._Out != 0u) ? 1u : 0u);   // включаем вентилятор вытяжки

    const uint8_t allPowerLoadsOff =    // все ТЭНы и вентиляторы выключены
        (g.stagedTen1LeftOn == 0u && g.stagedTen2LeftOn == 0u &&
         g.stagedTen1RightOn == 0u && g.stagedTen2RightOn == 0u &&
         g.stagedVent1LeftOn == 0u && g.stagedVent2LeftOn == 0u &&
         g.stagedVent1RightOn == 0u && g.stagedVent2RightOn == 0u) ? 1u : 0u;   // если все ТЭНы и вентиляторы выключены, то PowerLoadsOff = 1

    const ShutdownStage shutdownStage = (ShutdownStage)g.shutdownStage;   // текущий шаг процесса завершения работы

    if (shutdownStage == ShutdownStage::StopActuatorsAndGatePulse)   // если текущий шаг процесса завершения работы StopActuatorsAndGatePulse
    {
        // Шаг 1: только после полной остановки ТЭНов/вентиляторов приоткрываем ворота для вентиляции (Open на 3 с).
        // Пока allPowerLoadsOff==0 — только ApplyOutputs выше поэтапно гасит нагрузки; импульс ворот не трогаем.
        if (allPowerLoadsOff != 0u)
        {
            if (g.shutdownGateOpening == 0u)
            // если ворота не находятся в процессе открытия
            {   
                GateControl_SetCommand(GateControlCommand::Open, 1u);   // даем сигнал на открывание ворот
                g.shutdownGateOpening = 1u;   // устанавливаем флаг "ворота открываются"
                g.shutdownGateLiftPulse_s = 3u;   // устанавливаем время открытия ворот на 3 секунды
            } 
            // если ворота находятся в процессе открытия
            else if (g.shutdownGateLiftPulse_s > 0u)    // И если время открытия ворот не равно 0
            {
                g.shutdownGateLiftPulse_s--;   // уменьшаем время открытия ворот
                if (g.shutdownGateLiftPulse_s == 0u)
                {   // если время открытия ворот равно 0, ворота приоткрылись, сбрасываем флаги и переходим к шагу Ventilation
                    g.shutdownGateOpening = 0u;   // сбрасываем флаг "ворота открываются"
                    GateControl_SetCommand(GateControlCommand::Open, 0u);   // снимаем сигнал на открывание ворот
                    g.shutdownStage = (uint8_t)ShutdownStage::Ventilation;   // устанавливаем шаг процесса завершения работы - Ventilation
                    g.shutdownFlapOpenWait_s = 0u;   // сбрасываем время ожидания открытия заслонки
                }
            }
        }
    }
    else if (shutdownStage == ShutdownStage::Ventilation)
    {
        // Шаг 2: концевики заслонки по DI; таймаут — g.flapAlarm в UpdateDeviceAlarmState() или счётчик при отладке без DeviceSwitchCheck.
        // Таймаут без Air_Open: вытяжку не включаем — сразу FullGateOpen (см. ShutdownGoToFullGateOpen_VentCleanup).
        // После открытия заслонки: продувка по таймеру 300 с и/или по влажности возврата (RH < 50%).
        // При включённой проверке устройств: при _Out=1 учитываем бит Vent_Out — при аварии без продувки в FullGateOpen.
        // При выключенной проверке Vent_Out не используем, продувка так же по таймеру и влажности.
        Model::DFR.Water_Flap = 1u;
        const uint8_t airOpen = (Model::DI_DFR.Bits.Air_Open != 0u) ? 1u : 0u;  // сигнал от концевика Air_Open
        const uint8_t airClose = (Model::DI_DFR.Bits.Air_Close != 0u) ? 1u : 0u;  // сигнал от концевика Air_Close
        const uint8_t flapOpened = (airOpen != 0u && airClose == 0u) ? 1u : 0u;  // заслонка открыта
        const uint8_t switchCheckOn = (DefrostControl_IsDeviceSwitchCheckEnabled() != 0u) ? 1u : 0u;  // включена ли проверка устройств
        const uint8_t flapTimedOut = (switchCheckOn != 0u)   // если проверка устройств:
            ? ((g.flapAlarm != 0u) ? 1u : 0u)   // включена - flapTimedOut = внутренний флаг аварии заслонки
            : ((g.shutdownFlapOpenWait_s >= DEFROST_FLAP_POSITION_TIMEOUT_S) ? 1u : 0u);   // выключена - flapTimedOut = таймаут заслонки по времени

        if (flapTimedOut != 0u && flapOpened == 0u)
        // если авария заслонки (при включенной проверке устройств) ИЛИ заслонка не открылась (при таймауте при выключенной проверке устройств), 
        // то переходим к шагу полного открытия ворот без продувки
        {
            // Фиксируем аварийный бит заслонки в регистре аварий.
            g.flapAlarm = 1u;   // это важно при выключенной проверке устройств
            Model::Device_AlarmFlags |= (uint16_t)(1u << 11);   // устанавливаем бит аварии заслонки в регистре аварий
            ShutdownGoToFullGateOpen_VentCleanup();   // переходим к шагу полного открытия ворот без продувки
        }
        // заслонка открывается, ждем подтверждения открытия заслонки
        else if (flapOpened == 0u) {    // если заслонка ещё не открылась, но в процессе открытия
            // Без подтверждения Air_Open вытяжной вентилятор не должен работать.
            g.outFanOn = 0u;
            g.shutdownOutFanRemain_s = 0u;
            Model::DFR._Out = 0u;
            // ничего не делаем, ждём, т.к. при включенной проверке проверяется таймаут в UpdateDeviceAlarmState() — флаг g.flapAlarm
            // НО:
            if (switchCheckOn == 0u) {   // если проверка устройств выключена, то проверяем таймаут по времени
                if (g.shutdownFlapOpenWait_s < DEFROST_FLAP_POSITION_TIMEOUT_S)    // если время ожидания открытия заслонки ещё меньше таймаута
                {
                    ++g.shutdownFlapOpenWait_s;   // ожидаем и увеличиваем время ожидания открытия заслонки
                }
            }
        }
        else    // заслонка открылась
        {
            const uint16_t ventOutAlarm =   // сигнал аварии вентилятора Vent_Out (только при проверке устройств)
                (uint16_t)(Model::Device_AlarmFlags & kDeviceAlarmVentOutBit);

            if (Model::DFR._Out == 0u)    // если вентилятор вытяжки выключен
            {
                Model::DFR._Out = 1u;   // включаем вентилятор вытяжки
                // До проверки аварии Vent_Out на следующем такте (DeviceSwitchCheck в ModBus) таймер продувки не ведём (remain=0, outFanOn=0).
                g.shutdownOutFanRemain_s = 0u;   // сбрасываем таймер продувки
            }
            else    // если вентилятор вытяжки включен
            {
                if (switchCheckOn != 0u && ventOutAlarm != 0u)   // авария Vent_Out только при включённой проверке устройств
                {
 //                   HAL_GPIO_WritePin(GPIOG, LD4_Pin, GPIO_PIN_SET);
                    ShutdownGoToFullGateOpen_VentCleanup();   // переходим к шагу полного открытия ворот без продувки
                }
                else    // если авария Vent_Out не произошла
                {
                    // Продувка: таймер 300 с и/или влажность воздуха на вытяжке (RH < 50%).
                    if (g.shutdownOutFanRemain_s == 0u && g.outFanOn == 0u)
                    {
                        g.outFanOn = 1u;   // в состоянии дефростера отмечаем, что вентилятор вытяжки включен
                        g.shutdownOutFanRemain_s = 300u;   // устанавливаем таймер продувки на 300 секунд
                    }

                    const float RH_ret = DeciToRH((int16_t)Model::getCurrentVal_H(kSensReturn_T_H));   // влажность воздуха на вытяжке
                    if (g.shutdownOutFanRemain_s > 0u)   // если таймер продувки больше 0
                    {
                        g.shutdownOutFanRemain_s--;   // уменьшаем таймер продувки
                    }

                    if (g.shutdownOutFanRemain_s == 0u || RH_ret < 50.0f)   // если таймер продувки равен 0 ИЛИ влажность воздуха на вытяжке меньше 50%
                    {
                        g.outFanOn = 0u;   // в состоянии дефростера отмечаем, что вентилятор вытяжки выключен
                        g.shutdownOutFanRemain_s = 0u;   // сбрасываем таймер продувки
                        Model::DFR._Out = 0u;   // выключаем вентилятор вытяжки
                        Model::DFR.Water_Flap = 0u;   // закрываем заслонку
                        g.shutdownFlapOpenWait_s = 0u;   // сбрасываем время ожидания открытия заслонки
                        ShutdownEnterFullGateOpenArm();   // переходим к шагу полного открытия ворот
                    }
                }
            }
        }
    }
    else
    {
        // Шаг 3: полное открытие ворот и параллельно ожидание закрытия заслонки (Water_Flap=0 с конца продувки).
        // Завершение шага — когда привод ворот отработал команду Open (снята с шины) И заслонка закрыта по DI
        // либо истёк таймаут заслонки (flapAlarm / счётчик без DeviceSwitchCheck).
        Model::DFR.Water_Flap = 0u;

        if (g.shutdownGateFullOpenActive == 0u) {
            ShutdownEnterFullGateOpenArm();   // переходим к шагу полного открытия ворот
        }

        const uint8_t switchCheckOn = (DefrostControl_IsDeviceSwitchCheckEnabled() != 0u) ? 1u : 0u;   // включена ли проверка устройств
        const uint8_t airOpen = (Model::DI_DFR.Bits.Air_Open != 0u) ? 1u : 0u;   // сигнал от концевика Air_Open
        const uint8_t airClose = (Model::DI_DFR.Bits.Air_Close != 0u) ? 1u : 0u;   // сигнал от концевика Air_Close 
        const uint8_t flapClosedOk = (airClose != 0u && airOpen == 0u) ? 1u : 0u;   // заслонка закрыта

        uint8_t flapCloseWaitDone = 0u;
        if (switchCheckOn != 0u)   // если проверка устройств включена
        {
            flapCloseWaitDone = ((flapClosedOk != 0u) || (g.flapAlarm != 0u)) ? 1u : 0u;   // заслонка закрыта или авария заслонки
        }
        else   // если проверка устройств выключена
        {
            if (g.shutdownFlapOpenWait_s < DEFROST_FLAP_POSITION_TIMEOUT_S)   // если время ожидания закрытия заслонки ещё меньше таймаута
            {
                ++g.shutdownFlapOpenWait_s;   // ожидаем и увеличиваем время ожидания закрытия заслонки
            }
            flapCloseWaitDone = (g.shutdownFlapOpenWait_s >= DEFROST_FLAP_POSITION_TIMEOUT_S) ? 1u : 0u;   
            // заслонка закрыта условно, время закрытия выдержано по таймауту
        }

        if (g.shutdownGateFullOpenActive != 0u)   
        // Шаг полного открытия ворот идёт параллельно с ожиданием закрытия заслонки
        {
            if (g.shutdownGateOpenPhase_s < 255u)   // если время ожидания полного открытия ворот ещё меньше 255 секунд
            {
                ++g.shutdownGateOpenPhase_s;   // ожидаем и увеличиваем время ожидания полного открытия ворот
            }
            const uint8_t openCmdActive =   // сигнал активной команды Open на ворота
                (GateControl_IsCommandActive(GateControlCommand::Open) != 0u) ? 1u : 0u;
            if (openCmdActive != 0u)   // если команда Open на ворота активна
            {
                g.shutdownGateOpenSawActive = 1u;   // устанавливаем флаг активной команды Open на ворота
            }
            
            if (g.shutdownGateOpenReleased == 0u)   // если команда Open на ворота не выполнена до конца
            {
                // Не снимать Open в тот же такт, что Arm(): GateControl ещё не видел команду (DataFunc: Defrost после GateControl).
                const uint8_t openIdle = (openCmdActive == 0u) ? 1u : 0u;
                const uint8_t gateDriveFinished =   // команда Open на ворота выполнена до конца
                    ((g.shutdownGateOpenSawActive != 0u) && (openIdle != 0u)) ? 1u : 0u;
                const uint8_t gateStuckFallback =   // команда Open на ворота выполнена по таймауту
                    ((g.shutdownGateOpenSawActive == 0u) && (g.shutdownGateOpenPhase_s >= kShutdownGateOpenStuckTimeout_s))
                        ? 1u
                        : 0u;
                
                // Команда Open на ворота выполнена до конца или по таймауту
                if ((gateDriveFinished != 0u) || (gateStuckFallback != 0u))   
                {
                    GateControl_SetCommand(GateControlCommand::Open, 0u);   // снимаем команду Open на ворота
                    g.shutdownGateOpenReleased = 1u;   // устанавливаем флаг выполнения команды Open на ворота
                }
            }
        }

        if ((g.shutdownGateOpenReleased != 0u) && (flapCloseWaitDone != 0u))   
        // если команда Open на ворота выполнена до конца и заслонка закрыта
        {
            g.shutdownGateFullOpenActive = 0u;   // сбрасываем флаг шага полного открытия ворот
        }
    }
}

static void StartAutomaticSequence()
{
    ResetState();
    // Запуск автоматического алгоритма должен выполняться именно в автоматическом режиме.
    GateControl_SetManualMode(0);
    Model::setDefrostManualModeEnabled(false);

    // Сбрасываем остатки post-shutdown и фиксируем рабочее начальное состояние.
    g.shutdownActive = 0;
    g.shutdownGateOpening = 0;
    g.shutdownGateFullOpenActive = 0;
    g.shutdownGateOpenReleased = 0;
    g.shutdownGateOpenSawActive = 0;
    g.shutdownGateOpenPhase_s = 0;
    g.shutdownStage = (uint8_t)ShutdownStage::StopActuatorsAndGatePulse;
    g.shutdownFlapOpenWait_s = 0;
    g.shutdownOutFanRemain_s = 0;
    g.outFanOn = 0;
    g.outOn = 0;
    g.outDamperState = 0;
    g.outDamperTimer_s = 0;
    g.startPendingAfterShutdown = 0;
    Model::DFR._Out = 0;
    Model::DFR.Water_Flap = 0; // В рабочем (автоматическом) режиме заслонка закрыта.

    // Сбрасываем команды ворот перед формированием команды на старт.
    GateControl_SetCommand(GateControlCommand::Open, 0);
    GateControl_SetCommand(GateControlCommand::Close, 0);
    GateControl_SetCommand(GateControlCommand::Deblock, 0);

    ApplyModeLamps(LampModeState::AutoActive);

    // На старте авто-режима закрываем ворота, если они не в нижнем положении.
    if (GateControl_IsClosedPosition() == 0)
    {
        GateControl_SetCommand(GateControlCommand::Close, 1);
        g.startupGateClosing = 1;
        ApplyOutputs(0, 0, 0, 0, 0, 0, 0, 0);  // один раз: все выходы выкл. до закрытия ворот
    }
    else
    {
        g.startupGateClosing = 0;
        GateControl_SetCommand(GateControlCommand::Close, 0);
    }

    g.shutdownActive = 0;
    g.shutdownOutFanRemain_s = 0;
    g.enabled = 1;
}

static uint8_t DefrostControl_GetParamInternal(uint8_t groupId, uint8_t paramId, DefrostParamValue_t *outValue)
{
    if (outValue == nullptr)
    {
        return 0;
    }

    switch (groupId)
    {
        case DEFROST_PARAM_GROUP_SENSORS:
            if (paramId < kDefrostSensorCount)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U8;
                outValue->value.u8 = g_defrostParams.sensorUseInDefrost[paramId];
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_TEMPERATURE:
            if (paramId <= 2 && IsPhaseIndexValid(paramId))
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.fishHotMax_C[paramId];
                return 1;
            }
            if (paramId >= 3 && paramId <= 5 && IsPhaseIndexValid((uint8_t)(paramId - 3)))
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.supplySet_C[paramId - 3];
                return 1;
            }
            if (paramId >= 6 && paramId <= 8 && IsPhaseIndexValid((uint8_t)(paramId - 6)))
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.supplyMax_C[paramId - 6];
                return 1;
            }
            if (paramId >= 9 && paramId <= 11 && IsPhaseIndexValid((uint8_t)(paramId - 9)))
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.fishDeltaMax_C[paramId - 9];
                return 1;
            }
            if (paramId >= 12 && paramId <= 14 && IsPhaseIndexValid((uint8_t)(paramId - 12)))
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.fishHotRateMax_Cps[paramId - 12];
                return 1;
            }
            if (paramId == 15)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.leftRightTrimGain;
                return 1;
            }
            if (paramId == 16)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.leftRightTrimMaxEq;
                return 1;
            }
            if (paramId == 17)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.piKp;
                return 1;
            }
            if (paramId == 18)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.piKi;
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_HUMIDITY:
            if (paramId <= 2 && IsPhaseIndexValid(paramId))
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.returnTargetRH_percent[paramId];
                return 1;
            }
            if (paramId == 3)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.wDeadband_kgkg;
                return 1;
            }
            if (paramId == 4)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.outDamperTimer_s;
                return 1;
            }
            if (paramId == 5)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.outFanDelay_s;
                return 1;
            }
            if (paramId == 6)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                outValue->value.f32 = g_defrostParams.injGain;
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_PWM:
            if (paramId == 0)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.tenMinHold_s;
                return 1;
            }
            if (paramId == 1)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.injMinHold_s;
                return 1;
            }
            if (paramId == 2)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.outHold_s;
                return 1;
            }
            if (paramId == 3)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.airOnlyPhaseWarmUp_s;
                return 1;
            }
            if (paramId == 4)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.airOnlyPhasePlateau_s;
                return 1;
            }
            if (paramId == 5)
            {
                outValue->valueType = DEFROST_PARAM_TYPE_U16;
                outValue->value.u16 = g_defrostParams.maxRuntime_s;
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_LOG_PHASE:
            /* Группа 1 лога: параметры для всех фаз. paramId 0..5 = фаза 0, 6..11 = фаза 1, 12..17 = фаза 2 (в каждой фазе: fishHotMax_C, fishHotRateMax_Cps, fishDeltaMax_C, supplyMax_C, supplySet_C, returnTargetRH_percent). */
            if (paramId < DEFROST_PHASE_COUNT * 6u)
            {
                const uint8_t phaseIdx = paramId / 6u;
                const uint8_t subId   = paramId % 6u;
                outValue->valueType = DEFROST_PARAM_TYPE_F32;
                switch (subId)
                {
                    case 0: outValue->value.f32 = g_defrostParams.fishHotMax_C[phaseIdx];       break;
                    case 1: outValue->value.f32 = g_defrostParams.fishHotRateMax_Cps[phaseIdx]; break;
                    case 2: outValue->value.f32 = g_defrostParams.fishDeltaMax_C[phaseIdx];     break;
                    case 3: outValue->value.f32 = g_defrostParams.supplyMax_C[phaseIdx];       break;
                    case 4: outValue->value.f32 = g_defrostParams.supplySet_C[phaseIdx];        break;
                    case 5: outValue->value.f32 = g_defrostParams.returnTargetRH_percent[phaseIdx]; break;
                    default: return 0;
                }
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_LOG_GLOBAL:
            /* Группа 2 лога: параметры, общие для всех фаз. */
            if (paramId == 0) { outValue->valueType = DEFROST_PARAM_TYPE_F32; outValue->value.f32 = g_defrostParams.leftRightTrimGain; return 1; }
            if (paramId == 1) { outValue->valueType = DEFROST_PARAM_TYPE_F32; outValue->value.f32 = g_defrostParams.leftRightTrimMaxEq; return 1; }
            if (paramId == 2) { outValue->valueType = DEFROST_PARAM_TYPE_F32; outValue->value.f32 = g_defrostParams.piKp; return 1; }
            if (paramId == 3) { outValue->valueType = DEFROST_PARAM_TYPE_F32; outValue->value.f32 = g_defrostParams.piKi; return 1; }
            if (paramId == 4) { outValue->valueType = DEFROST_PARAM_TYPE_F32; outValue->value.f32 = g_defrostParams.wDeadband_kgkg; return 1; }
            if (paramId == 5) { outValue->valueType = DEFROST_PARAM_TYPE_F32; outValue->value.f32 = g_defrostParams.injGain; return 1; }
            if (paramId == 6) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.outDamperTimer_s; return 1; }
            if (paramId == 7) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.outFanDelay_s; return 1; }
            if (paramId == 8) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.outHold_s; return 1; }
            if (paramId == 9) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.tenMinHold_s; return 1; }
            if (paramId == 10) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.injMinHold_s; return 1; }
            if (paramId == 11) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.airOnlyPhaseWarmUp_s; return 1; }
            if (paramId == 12) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.airOnlyPhasePlateau_s; return 1; }
            if (paramId == 13) { outValue->valueType = DEFROST_PARAM_TYPE_U16; outValue->value.u16 = g_defrostParams.maxRuntime_s; return 1; }
            if (paramId == 14) { outValue->valueType = DEFROST_PARAM_TYPE_F32; outValue->value.f32 = g_defrostParams.fishColdTarget_C; return 1; }
            if (paramId == 15) { outValue->valueType = DEFROST_PARAM_TYPE_U8; outValue->value.u8 = g_defrostParams.debugDisableTargetTStop; return 1; }
            if (paramId == 16) { outValue->valueType = DEFROST_PARAM_TYPE_U8; outValue->value.u8 = g_defrostParams.debugDisableDeviceSwitchCheck; return 1; }
            if (paramId < 17u + DEFROST_MAX_SENSOR_COUNT)
            {
                const uint8_t idx = (uint8_t)(paramId - 17u);
                outValue->valueType = DEFROST_PARAM_TYPE_U8;
                outValue->value.u8 = g_defrostParams.sensorUseInDefrost[idx];
                return 1;
            }
            return 0;

        default:
            return 0;
    }
}

static uint8_t DefrostControl_SetParamInternal(uint8_t groupId, uint8_t paramId, const DefrostParamValue_t *inValue)
{
    if (inValue == nullptr)
    {
        return 0;
    }

    switch (groupId)
    {
        case DEFROST_PARAM_GROUP_SENSORS:
            if (paramId < kDefrostSensorCount && inValue->valueType == DEFROST_PARAM_TYPE_U8)
            {
                g_defrostParams.sensorUseInDefrost[paramId] = (inValue->value.u8 != 0u) ? 1u : 0u;
                Sensor_array[paramId].UseInDefrost = g_defrostParams.sensorUseInDefrost[paramId];
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_TEMPERATURE:
            if (inValue->valueType != DEFROST_PARAM_TYPE_F32)
            {
                return 0;
            }
            if (paramId <= 2 && IsPhaseIndexValid(paramId))
            {
                g_defrostParams.fishHotMax_C[paramId] = Clamp(inValue->value.f32, 0.0f, 50.0f);
                return 1;
            }
            if (paramId >= 3 && paramId <= 5 && IsPhaseIndexValid((uint8_t)(paramId - 3)))
            {
                g_defrostParams.supplySet_C[paramId - 3] = Clamp(inValue->value.f32, 0.0f, 50.0f);
                return 1;
            }
            if (paramId >= 6 && paramId <= 8 && IsPhaseIndexValid((uint8_t)(paramId - 6)))
            {
                g_defrostParams.supplyMax_C[paramId - 6] = Clamp(inValue->value.f32, 0.0f, 70.0f);
                return 1;
            }
            if (paramId >= 9 && paramId <= 11 && IsPhaseIndexValid((uint8_t)(paramId - 9)))
            {
                g_defrostParams.fishDeltaMax_C[paramId - 9] = Clamp(inValue->value.f32, 0.0f, 30.0f);
                return 1;
            }
            if (paramId >= 12 && paramId <= 14 && IsPhaseIndexValid((uint8_t)(paramId - 12)))
            {
                g_defrostParams.fishHotRateMax_Cps[paramId - 12] = Clamp(inValue->value.f32, 0.0001f, 1.0f);
                return 1;
            }
            if (paramId == 15)
            {
                g_defrostParams.leftRightTrimGain = Clamp(inValue->value.f32, 0.0f, 1.0f);
                g.leftRightTrimGain = g_defrostParams.leftRightTrimGain;
                return 1;
            }
            if (paramId == 16)
            {
                g_defrostParams.leftRightTrimMaxEq = Clamp(inValue->value.f32, 0.0f, 2.0f);
                return 1;
            }
            if (paramId == 17)
            {
                g_defrostParams.piKp = Clamp(inValue->value.f32, 0.01f, 2.0f);
                return 1;
            }
            if (paramId == 18)
            {
                g_defrostParams.piKi = Clamp(inValue->value.f32, 0.001f, 0.5f);
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_HUMIDITY:
            if (paramId <= 3 && inValue->valueType == DEFROST_PARAM_TYPE_F32)
            {
                if (paramId <= 2 && IsPhaseIndexValid(paramId))
                {
                    g_defrostParams.returnTargetRH_percent[paramId] = Clamp(inValue->value.f32, 10.0f, 100.0f);
                    return 1;
                }
                if (paramId == 3)
                {
                    g_defrostParams.wDeadband_kgkg = Clamp(inValue->value.f32, 0.0001f, 0.0100f);
                    g.wDeadband_kgkg = g_defrostParams.wDeadband_kgkg;
                    return 1;
                }
            }
            if (paramId == 4 && inValue->valueType == DEFROST_PARAM_TYPE_U16)
            {
                g_defrostParams.outDamperTimer_s = inValue->value.u16;
                return 1;
            }
            if (paramId == 5 && inValue->valueType == DEFROST_PARAM_TYPE_U16)
            {
                g_defrostParams.outFanDelay_s = inValue->value.u16;
                g.outFanDelay_s = g_defrostParams.outFanDelay_s;
                return 1;
            }
            if (paramId == 6 && inValue->valueType == DEFROST_PARAM_TYPE_F32)
            {
                g_defrostParams.injGain = Clamp(inValue->value.f32, 100.0f, 2000.0f);
                return 1;
            }
            return 0;

        case DEFROST_PARAM_GROUP_PWM:
            if (inValue->valueType != DEFROST_PARAM_TYPE_U16)
            {
                return 0;
            }
            if (paramId == 0)
            {
                g_defrostParams.tenMinHold_s = inValue->value.u16;
                return 1;
            }
            if (paramId == 1)
            {
                g_defrostParams.injMinHold_s = inValue->value.u16;
                return 1;
            }
            if (paramId == 2)
            {
                g_defrostParams.outHold_s = inValue->value.u16;
                return 1;
            }
            if (paramId == 3)
            {
                g_defrostParams.airOnlyPhaseWarmUp_s = (uint16_t)Clamp((float)inValue->value.u16, 60.0f, 7200.0f);
                return 1;
            }
            if (paramId == 4)
            {
                g_defrostParams.airOnlyPhasePlateau_s = (uint16_t)Clamp((float)inValue->value.u16, 60.0f, 14400.0f);
                return 1;
            }
            if (paramId == 5)
            {
                g_defrostParams.maxRuntime_s = (uint16_t)Clamp((float)inValue->value.u16, 300.0f, 65535.0f);
                return 1;
            }
            return 0;

        default:
            return 0;
    }
}

static uint8_t SerializeParamEntry(uint8_t paramId, const DefrostParamValue_t *value, uint8_t *outData, uint8_t capacity, uint8_t *inOutOffset)
{
    if (value == nullptr || outData == nullptr || inOutOffset == nullptr)
    {
        return 0;
    }

    uint8_t valueSize = 0;
    if (value->valueType == DEFROST_PARAM_TYPE_U8) valueSize = 1;
    else if (value->valueType == DEFROST_PARAM_TYPE_U16) valueSize = 2;
    else if (value->valueType == DEFROST_PARAM_TYPE_F32) valueSize = 4;
    else return 0;

    const uint8_t needed = (uint8_t)(3 + valueSize);
    if ((uint8_t)(capacity - *inOutOffset) < needed)
    {
        return 0;
    }

    uint8_t *p = &outData[*inOutOffset];
    p[0] = paramId;
    p[1] = value->valueType;
    p[2] = valueSize;
    if (value->valueType == DEFROST_PARAM_TYPE_U8)
    {
        p[3] = value->value.u8;
    }
    else if (value->valueType == DEFROST_PARAM_TYPE_U16)
    {
        memcpy(&p[3], &value->value.u16, 2);
    }
    else
    {
        memcpy(&p[3], &value->value.f32, 4);
    }
    *inOutOffset = (uint8_t)(*inOutOffset + needed);
    return 1;
}

 extern "C"
 {
     void DefrostControl_Init(void)
     {
        DefrostControl_LoadParams();    // загрузка параметров из EEPROM
        ResetState();  // сброс состояния ШИМ алгоритма дефростера
     }
 
     void DefrostControl_SetEnabled(uint8_t enabled)
     {
         // Почему: отдельный флаг enabled позволяет безопасно вводить алгоритм, не ломая существующую логику авто/ручного режима.
       UpdateDeviceAlarmState();
        const uint8_t newEnabled = enabled ? 1 : 0;
        const uint8_t wasEnabled = g.enabled;

        if (newEnabled != 0)
        {
            // Если ещё идёт post-shutdown (продувка/открытие ворот), откладываем новый START.
            if (g.shutdownActive != 0u || g.shutdownGateOpening != 0u || g.shutdownGateFullOpenActive != 0u ||
                g.shutdownOutFanRemain_s != 0u || g.outFanOn != 0u || g.outDamperState != 0u)
            {
                g.startPendingAfterShutdown = 1u;
                g.enabled = 0u;
                ApplyModeLamps(LampModeState::StoppedOrManual);
                return;
            }

            StartAutomaticSequence();
            g_defrostPersistedAutoMode = 1u;
            PersistParamsToEepromIfAvailable(); // сохранение параметров в EEPROM, если EEPROM доступна
            return;
        }

        g.enabled = 0;
        g.startPendingAfterShutdown = 0u; // Явный STOP отменяет любой отложенный автозапуск.
        // Важно: последовательность останова запускаем только по переходу 1 -> 0.
        // Иначе при повторных DefrostControl_SetEnabled(0) команда Gate_Up зацикливается.
        if (wasEnabled != 0u)
        {
            ShutdownSequence();  // Безопасное выключение всех элементов
            g.runtimeSeconds = 0; // Время работы алгоритма обнуляется при остановке
            // Останов: зелёная лампа выключена, красная только по аварии.
            ApplyModeLamps(LampModeState::StoppedOrManual);
        }
        g_defrostPersistedAutoMode = 0u;
        PersistParamsToEepromIfAvailable(); // сохранение параметров в EEPROM, если EEPROM доступна
    }

    uint32_t DefrostControl_GetRuntimeSeconds(void)
    {
        return g.runtimeSeconds;
    }

    uint8_t DefrostControl_IsEnabled(void)
    {
        // Почему: в HOME нужен признак именно активного автоматического режима,
        // а не просто факт, что алгоритм ранее был запущен.
        return (g.enabled != 0 && GateControl_GetManualMode() == 0) ? 1 : 0;
    }

    uint8_t DefrostControl_IsShutdownActive(void)
    {
        return (g.shutdownActive != 0u) ? 1u : 0u;
    }

    uint8_t DefrostControl_GetParam(uint8_t groupId, uint8_t paramId, DefrostParamValue_t *outValue)
    {
        return DefrostControl_GetParamInternal(groupId, paramId, outValue);
    }

    uint8_t DefrostControl_SetParam(uint8_t groupId, uint8_t paramId, const DefrostParamValue_t *inValue)
    {
        const uint8_t ok = DefrostControl_SetParamInternal(groupId, paramId, inValue);
        if (ok != 0u)
        {
            DefrostControl_SaveParams();
        }
        return ok;
    }

    uint8_t DefrostControl_GetGroup(uint8_t groupId, uint8_t page, uint8_t *outData, uint8_t outCapacity, uint8_t *outLength)
    {
        (void)page;
        if (outData == nullptr || outLength == nullptr)
        {
            return 0;
        }

        /* Группы 5 и 6: фиксированная структура — копирование куска памяти из g_defrostParams. */
        if (groupId == DEFROST_PARAM_GROUP_LOG_PHASE)
        {
            const uint32_t sz = (uint32_t)sizeof(DefrostLogPhasePayload_t);
            if (outCapacity < sz)
            {
                return 0;
            }
            memcpy(outData, &g_defrostParams.fishHotMax_C, sz);
            *outLength = (uint8_t)sz;
            return 1;
        }
        if (groupId == DEFROST_PARAM_GROUP_LOG_GLOBAL)
        {
            DefrostLogGlobalPayload_t p;
            p.leftRightTrimGain   = g_defrostParams.leftRightTrimGain;
            p.leftRightTrimMaxEq  = g_defrostParams.leftRightTrimMaxEq;
            p.piKp                = g_defrostParams.piKp;
            p.piKi                = g_defrostParams.piKi;
            p.wDeadband_kgkg      = g_defrostParams.wDeadband_kgkg;
            p.injGain             = g_defrostParams.injGain;
            p.outDamperTimer_s    = g_defrostParams.outDamperTimer_s;
            p.outFanDelay_s       = g_defrostParams.outFanDelay_s;
            p.outHold_s           = g_defrostParams.outHold_s;
            p.tenMinHold_s        = g_defrostParams.tenMinHold_s;
            p.injMinHold_s        = g_defrostParams.injMinHold_s;
            p.airOnlyPhaseWarmUp_s  = g_defrostParams.airOnlyPhaseWarmUp_s;
            p.airOnlyPhasePlateau_s = g_defrostParams.airOnlyPhasePlateau_s;
            p.maxRuntime_s        = g_defrostParams.maxRuntime_s;
            p.fishColdTarget_C    = g_defrostParams.fishColdTarget_C;
            p.debugDisableTargetTStop = g_defrostParams.debugDisableTargetTStop;
            p.debugDisableDeviceSwitchCheck = g_defrostParams.debugDisableDeviceSwitchCheck;
            memcpy(p.sensorUseInDefrost, g_defrostParams.sensorUseInDefrost, sizeof(p.sensorUseInDefrost));
            const uint32_t sz = (uint32_t)sizeof(DefrostLogGlobalPayload_t);
            if (outCapacity < sz)
            {
                return 0;
            }
            memcpy(outData, &p, sz);
            *outLength = (uint8_t)sz;
            return 1;
        }

        uint8_t offset = 0;
        DefrostParamValue_t value;
        memset(&value, 0, sizeof(value));

        if (groupId == DEFROST_PARAM_GROUP_SENSORS)
        {
            for (uint8_t id = 0; id < kDefrostSensorCount; ++id)
            {
                if (DefrostControl_GetParamInternal(groupId, id, &value) == 0u ||
                    SerializeParamEntry(id, &value, outData, outCapacity, &offset) == 0u)
                {
                    break;
                }
            }
        }
        else
        {
            for (uint8_t id = 0; id < 32; ++id)
            {
                if (DefrostControl_GetParamInternal(groupId, id, &value) == 0u)
                {
                    continue;
                }
                if (SerializeParamEntry(id, &value, outData, outCapacity, &offset) == 0u)
                {
                    break;
                }
            }
        }

        *outLength = offset;
        return 1;
    }

    uint8_t DefrostControl_SetGroupPayload(uint8_t groupId, const uint8_t *payload, uint8_t payloadLen)
    {
        if (payload == nullptr)
        {
            return 0;
        }
        if (groupId == DEFROST_PARAM_GROUP_LOG_PHASE)
        {
            const uint32_t sz = (uint32_t)sizeof(DefrostLogPhasePayload_t);
            if ((uint32_t)payloadLen < sz)
            {
                return 0;
            }
            DefrostLogPhasePayload_t p;
            memcpy(&p, payload, sz);
            memcpy(&g_defrostParams.fishHotMax_C, &p.fishHotMax_C, sizeof(p.fishHotMax_C));
            memcpy(&g_defrostParams.fishHotRateMax_Cps, &p.fishHotRateMax_Cps, sizeof(p.fishHotRateMax_Cps));
            memcpy(&g_defrostParams.fishDeltaMax_C, &p.fishDeltaMax_C, sizeof(p.fishDeltaMax_C));
            memcpy(&g_defrostParams.supplySet_C, &p.supplySet_C, sizeof(p.supplySet_C));
            memcpy(&g_defrostParams.supplyMax_C, &p.supplyMax_C, sizeof(p.supplyMax_C));
            memcpy(&g_defrostParams.returnTargetRH_percent, &p.returnTargetRH_percent, sizeof(p.returnTargetRH_percent));
            DefrostControl_SaveParams();
            return 1;
        }
        if (groupId == DEFROST_PARAM_GROUP_LOG_GLOBAL)
        {
            const uint32_t sz = (uint32_t)sizeof(DefrostLogGlobalPayload_t);
            if ((uint32_t)payloadLen < sz)
            {
                return 0;
            }
            DefrostLogGlobalPayload_t p;
            memcpy(&p, payload, sz);
            g_defrostParams.leftRightTrimGain   = p.leftRightTrimGain;
            g_defrostParams.leftRightTrimMaxEq  = p.leftRightTrimMaxEq;
            g_defrostParams.piKp                = p.piKp;
            g_defrostParams.piKi                = p.piKi;
            g_defrostParams.wDeadband_kgkg      = p.wDeadband_kgkg;
            g_defrostParams.injGain             = p.injGain;
            g_defrostParams.outDamperTimer_s    = p.outDamperTimer_s;
            g_defrostParams.outFanDelay_s       = p.outFanDelay_s;
            g_defrostParams.outHold_s           = p.outHold_s;
            g_defrostParams.tenMinHold_s        = p.tenMinHold_s;
            g_defrostParams.injMinHold_s        = p.injMinHold_s;
            g_defrostParams.airOnlyPhaseWarmUp_s  = p.airOnlyPhaseWarmUp_s;
            g_defrostParams.airOnlyPhasePlateau_s = p.airOnlyPhasePlateau_s;
            g_defrostParams.maxRuntime_s        = p.maxRuntime_s;
            g_defrostParams.fishColdTarget_C   = p.fishColdTarget_C;
            g_defrostParams.debugDisableTargetTStop = (p.debugDisableTargetTStop != 0u) ? 1u : 0u;
            g_defrostParams.debugDisableDeviceSwitchCheck = (p.debugDisableDeviceSwitchCheck != 0u) ? 1u : 0u;
            memcpy(g_defrostParams.sensorUseInDefrost, p.sensorUseInDefrost, sizeof(p.sensorUseInDefrost));
            for (uint8_t i = 0; i < kDefrostSensorCount; ++i)
            {
                Sensor_array[i].UseInDefrost = (g_defrostParams.sensorUseInDefrost[i] != 0u) ? 1u : 0u;
            }
            g.leftRightTrimGain = g_defrostParams.leftRightTrimGain;
            g.wDeadband_kgkg = g_defrostParams.wDeadband_kgkg;
            g.outFanDelay_s = g_defrostParams.outFanDelay_s;
            DefrostControl_SaveParams();
            return 1;
        }
        return 0;
    }

    void DefrostControl_GetParams(DefrostParams_t *outParams)
    {
        if (outParams == nullptr)
        {
            return;
        }
        memcpy(outParams, &g_defrostParams, sizeof(DefrostParams_t));
    }

    uint8_t DefrostControl_SetParams(const DefrostParams_t *inParams)
    {
        if (inParams == nullptr)
        {
            return 0;
        }

        memcpy(&g_defrostParams, inParams, sizeof(DefrostParams_t));
        for (uint8_t i = 0; i < kDefrostSensorCount; ++i)
        {
            Sensor_array[i].UseInDefrost = (g_defrostParams.sensorUseInDefrost[i] != 0u) ? 1u : 0u;
        }
        g.leftRightTrimGain = g_defrostParams.leftRightTrimGain;
        g.wDeadband_kgkg = g_defrostParams.wDeadband_kgkg;
        g.outFanDelay_s = g_defrostParams.outFanDelay_s;
        DefrostControl_SaveParams();
        return 1;
    }

    void DefrostControl_SaveParams(void)
    {
        PersistParamsToEepromIfAvailable();
    }

    void DefrostControl_RestoreDefaultParams(void)
    {
        LoadDefaultParams(&g_defrostParams);
        for (uint8_t i = 0; i < kDefrostSensorCount; ++i)
        {
            Sensor_array[i].UseInDefrost = (g_defrostParams.sensorUseInDefrost[i] != 0u) ? 1u : 0u;
        }
        g.leftRightTrimGain = g_defrostParams.leftRightTrimGain;
        g.wDeadband_kgkg = g_defrostParams.wDeadband_kgkg;
        g.outFanDelay_s = g_defrostParams.outFanDelay_s;
        DefrostControl_SaveParams();
    }

    void DefrostControl_LoadParams(void)
    {
        // Рабочие параметры всегда начинаем с дефолтов; устойчивое хранилище — только EEPROM
        // (RAM-буфер после сброса питания не инициализирован и для загрузки непригоден).
        LoadDefaultParams(&g_defrostParams);

        // Только Init+Read до старта RTOS. Запись (~130 байт, страницы по 16) откладываем:
        // иначе при пустой/битой EEPROM main блокируется на I2C3 до osKernelStart.
        g_defrostEepromAvailable = EEPROM::Init();
        g_defrostEepromPersistPending = false;  // флаг ожидания записи в EEPROM
        if (g_defrostEepromAvailable)
        {
            DefrostEepromStorage_t rec = {};  // инициализация структуры
            // чтение данных из EEPROM
            const HAL_StatusTypeDef readStatus = EEPROM::Read(
                kDefrostEepromBaseAddress, (uint8_t *)&rec, (uint16_t)sizeof(rec));
            const uint16_t payloadCrc = EepromPayloadCrc(&rec);
            const bool recValid = (readStatus == HAL_OK) &&
                                  (rec.version == kDefrostEepromVersion) &&
                                  (rec.payloadCrc == payloadCrc);
            if (recValid)  // если данные валидны
            {
                // Параметры всегда берём из валидной записи (не только при autoMode=1).
                memcpy(&g_defrostParams, &rec.params, sizeof(DefrostParams_t));
                g_defrostPersistedAutoMode = (rec.autoModeEnabled != 0u) ? 1u : 0u;
                // Сам автозапуск — после старта RTOS (Update1s), не в Init до osKernelStart.
                g_defrostAutoRestorePending = g_defrostPersistedAutoMode;
            }
            else  // если данные не валидны
            {
                // Пустая/битая EEPROM: синхронизируем после старта RTOS (Update1s).
                g_defrostPersistedAutoMode = 0u;  // сбрасываем флаг авторежима
                g_defrostAutoRestorePending = 0u;
                g_defrostEepromPersistPending = true;
            }
        }
        else  // если EEPROM не доступна
        {
            g_defrostPersistedAutoMode = 0u;  // сбрасываем флаг авторежима
            g_defrostAutoRestorePending = 0u;
        }

        // для каждого датчика устанавливаем флаг UseInDefrost в зависимости от значения UseInDefrost из массива Sensor_array
        for (uint8_t i = 0; i < kDefrostSensorCount; ++i)
        {
            Sensor_array[i].UseInDefrost = (g_defrostParams.sensorUseInDefrost[i] != 0u) ? 1u : 0u;
        }
    }

    float DefrostControl_GetFishColdTarget_C(void)
    {
        return g_defrostParams.fishColdTarget_C;
    }

    void DefrostControl_SetFishColdTarget_C(float val_C)
    {
        g_defrostParams.fishColdTarget_C = val_C;
        DefrostControl_SaveParams();
    }

    uint8_t DefrostControl_IsDeviceSwitchCheckEnabled(void)
    {
        return (g_defrostParams.debugDisableDeviceSwitchCheck == 0u) ? 1u : 0u;
    }

    void DefrostControl_NotifyFlapWaterDiMismatchFromIo(void)
    {
        g.flapAlarm = 1u;
        Model::Device_AlarmFlags |= (uint16_t)(1u << 11);
    }

    uint16_t DefrostControl_GetFlapTransitionElapsedSForSwitchCheck(void)
    {
        if (g_defrostParams.debugDisableDeviceSwitchCheck != 0u)
        {
            return 0u;
        }
        return (uint16_t)g.flapTransitionElapsed_s;
    }
 
     void DefrostControl_Update1s(void)
     {
        FlushPendingEepromPersistIfNeeded();
        UpdateProductSensorFallout();
        // Восстановление авторежима после сброса питания (autoModeEnabled из EEPROM).
        // Делаем здесь, а не в Init: нужны RTOS и уже считанные концевики ворот.
        if (g_defrostAutoRestorePending != 0u)
        {
            // Если есть флаг восстановления авторежима и алгоритм выключен, то запускаем автоматический режим из EEPROM
            g_defrostAutoRestorePending = 0u;
            if (g_defrostPersistedAutoMode != 0u && g.enabled == 0u)
            {
                StartAutomaticSequence();
                // EEPROM уже согласована (autoMode=1); повторная запись не требуется.
            }
        }
        UpdateDeviceAlarmState();
         // Уважаем ручной режим и внешний флаг enabled.
         // Почему: ручной режим должен быть главным; алгоритм не должен "бороться" с оператором.
         if (g.enabled == 0)    // если алгоритм выключен
         {
            // Алгоритм выключен, работает ручной режим или процесс завершения работы
            // В режиме останова зелёная лампа выключена; красная только по аварии.
            ApplyModeLamps(LampModeState::StoppedOrManual);

            if (g.shutdownActive != 0)    // если процесс завершения работы активен
             {
                ProcessShutdownStage1s();    // запускаем шаг процесса завершения работы

                if (g.shutdownStage == (uint8_t)ShutdownStage::FullGateOpen &&
                    g.shutdownGateFullOpenActive == 0u)    // если ворота полностью открыты
                 {
                    g.shutdownActive = 0;   // сбрасываем флаг завершения алгоритма (сигнал _Shd на сервере)
                    g.shutdownStage = (uint8_t)ShutdownStage::StopActuatorsAndGatePulse;   // сброс машины post-shutdown
                    GateControl_SetCommand(GateControlCommand::Open, 0);   // закрываем ворота
                    GateControl_SetCommand(GateControlCommand::Close, 0);  // закрываем ворота
                    GateControl_SetCommand(GateControlCommand::Deblock, 0); // деблокируем ворота
                    
                    // Когда пришла команда СТАРТ, а процесс был ещё не завершён, то устанавливается флаг startPendingAfterShutdown для отложенного старта
                    if (g.startPendingAfterShutdown != 0u)    // если нужно запустить автоматический режим после завершения работы
                    {
                        StartAutomaticSequence();    // запускаем автоматический режим из отложенного старта
                        return;
                    }
                 }
             }
           // В ручном режиме обе лампы должны быть выключены.
           ApplyModeLamps(LampModeState::StoppedOrManual);
         }         

         // Включен автоматический алгоритм
         else
         {
             ApplyModeLamps(LampModeState::AutoActive);
             ControlStep1s();
 
             // Счётчик времени "Отработано" увеличиваем только когда реально работает автоматика.
             if (g.runtimeSeconds < UINT32_MAX)
             {
                 g.runtimeSeconds++;
             }
             else
             {
                 g.runtimeSeconds = 0;
             }
         };
     }
 }

} // namespace

 // 3. The task DataAnalysis processing data from sensors
 void DataFunc()
 {
	/* Ждём семафор завершения считывания датчиков (его отпустит Data.cpp).
	 * osSemaphoreAcquire — уменьшает счётчик на 1, когда он уже больше нуля (забирает токен).
	 * Если счётчик 0, поток блокируется и ждёт, пока кто-то сделает Release
	 */
	osSemaphoreAcquire(SensorsReadDone_SemHandle, osWaitForever);

	// Сначала обновляем состояние ворот (концевики/тайм-ауты),
	// затем шаг автоматики, чтобы автоматика видела актуальное состояние ворот в этом же такте.
	GateControl_Update1s();
	DefrostControl_Update1s();
 }

#ifdef __cplusplus
extern "C" {
#endif
ControlLogPayload_t DefrostControl_GetControlLogPayload(void)
{
	ControlLogPayload_t out = {};
    memcpy(&out, &s_controlLogPayload, sizeof(ControlLogPayload_t));
    return out;
}
#ifdef __cplusplus
}
#endif
