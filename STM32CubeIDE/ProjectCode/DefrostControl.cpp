/*
 * DefrostControl.cpp
 *
 * Практичный алгоритм управления дефростером с двумя потоками:
 * - Два потока подачи (левый/правый) с 2 ТЭН + 2 вентилятор на сторону
 * - Одна точка возврата/смешения (чердак над всасывающим люком)
 * - Форсунка (впрыск воды) и вытяжка для управления влажностью
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
 #include "GateControl.hpp"
 #include <gui/model/Model.hpp>      // Model::getCurrentVal_* и битовый регистр Model::DFR
 #include "ModBus.hpp"

extern SENSOR_typedef_t Sensor_array[SQ];

typedef struct {
    uint16_t version;
    uint16_t payloadCrc;
    DefrostParams_t params;
} DefrostParamsStorage_t;

static DefrostParams_t g_defrostParams;
static DefrostParamsStorage_t g_defrostParamsStorage;
static const uint16_t kDefrostParamsVersion = 1;
static const uint8_t kDefrostSensorCount = (SQ < DEFROST_MAX_SENSOR_COUNT) ? SQ : DEFROST_MAX_SENSOR_COUNT;
 
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
 
     // Масштаб "в десятых" (UI делит на 10.0).
     // Почему: преобразование должно быть в одном месте, чтобы не смешивать "сырые" и инженерные единицы.
     constexpr float kDeciToUnit = 0.1f;
 
     // Допущения по психрометрии.
     // Почему: без датчика давления для расчёта абсолютной влажности приходится принимать номинальное давление.
     constexpr float kAirPressure_kPa = 101.325f;
 
     // Период обновления управления.
     // Почему: алгоритм рассчитан на дискретность 1 сек и использует ограничения по скорости изменения.
     constexpr float kDt_s = 1.0f;
 
     // Ограничения безопасности/качества (по умолчанию; требуют настройки под продукт).
     // Почему: ограничения качества должны доминировать над "оптимальностью", чтобы избежать необратимых дефектов.
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

    static uint16_t ParamsCrc16(const uint8_t *data, uint32_t size)
    {
        uint16_t crc = 0xFFFFu;
        for (uint32_t i = 0; i < size; ++i)
        {
            crc ^= data[i];
            for (uint8_t b = 0; b < 8; ++b)
            {
                if ((crc & 1u) != 0u)
                {
                    crc = (crc >> 1) ^ 0xA001u;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

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
 
     struct PI
     {
         float kp = 0.0f;
         float ki = 0.0f;
         float i = 0.0f;
 
         float Step(float error, float dt, float uMin, float uMax)
         {
             // Anti-windup: корректируем интегратор при насыщении выхода.
             // Почему: актуаторы насыщаются (0..2 "эквивалента ТЭНа"), разгон интегратора даёт длинные перерегулирования.
             const float p = kp * error;
             i += ki * error * dt;
             float u = p + i;
             if (u > uMax)
             {
                 u = uMax;
                 i = u - p;
             }
             else if (u < uMin)
             {
                 u = uMin;
                 i = u - p;
             }
             return u;
         }
     };
 
     struct SigmaDeltaPWM
     {
         float acc = 0.0f;
         uint8_t Step(float duty01)
         {
             // Почему: sigma-delta "размазывает" включения, приближая аналоговую скважность при обновлении 1 Гц.
             duty01 = Clamp(duty01, 0.0f, 1.0f);
             acc += duty01;
             if (acc >= 1.0f)
             {
                 acc -= 1.0f;
                 return 1;
             }
             return 0;
         }
 
         void Reset()
         {
             acc = 0.0f;
         }
     };
 
     struct HoldSwitch
     {
         uint8_t state = 0;
         uint16_t hold_s = 0;
 
         uint8_t Step(uint8_t desiredState, uint16_t minHold_s)
         {
             // Почему: 1-секундная дискретность + sigma-delta могут вызвать частые переключения,
             //         что плохо для силовых реле/контакторов и ухудшает повторяемость теплового режима.
             if (hold_s > 0)
             {
                 hold_s--;
                 return state;
             }
 
             desiredState = desiredState ? 1 : 0;
             if (desiredState != state)
             {
                 state = desiredState;
                 hold_s = minHold_s;
             }
             return state;
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
        uint16_t shutdownOutFanRemain_s = 0; // остаток времени работы вытяжки после остановки алгоритма

        uint8_t startupGateClosing = 0; // при старте: 1 если нужно закрыть ворота через API GateControl
        uint8_t shutdownActive = 0;     // при остановке: 1 пока выполняется последовательность остановки
        uint8_t shutdownGateOpening = 0; // при остановке: 1 если открытие ворот выполняется через API GateControl
        uint8_t startupActuatorDelay_s = 0; // пауза между последовательными включениями вентиляторов и ТЭНов
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
       g.startupActuatorDelay_s = 0;
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
     }
 
     static float DeciToC(int16_t deciC)  { return (float)deciC * kDeciToUnit; }
     static float DeciToRH(int16_t deciRH){ return (float)deciRH * kDeciToUnit; }
 
    enum class LampModeState : uint8_t
    {
        AutoActive = 0,
        StoppedOrManual = 1
    };

    static void ApplyModeLamps(LampModeState modeState)
    {
        // Why: централизуем правила индикации режима, чтобы исключить расхождения между ветками auto/manual/stop.
        if (modeState == LampModeState::AutoActive)
        {
            Model::DFR._Wrk = 1;
            Model::DFR._Stp = 0;
            Model::DFR_manual._Wrk = 0;
            Model::DFR_manual._Stp = 1;
            return;
        }

        Model::DFR._Wrk = 0;
        Model::DFR._Stp = 1;
        Model::DFR_manual._Wrk = 0;
        Model::DFR_manual._Stp = 1;
    }

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
        // с интервалом 2 секунды между включениями. Выключение оставляем мгновенным.
        const uint8_t kStartupInterval_s = 2;
        if (g.startupActuatorDelay_s > 0)
        {
            g.startupActuatorDelay_s--;
        }

        bool switchedOnThisTick = false;
        auto stageActuator = [&](uint8_t desiredOn, uint8_t& stagedOn)
        {
            if (desiredOn == 0)
            {
                stagedOn = 0;
                return;
            }
            if (stagedOn != 0)
            {
                return;
            }
            if (!switchedOnThisTick && g.startupActuatorDelay_s == 0)
            {
                stagedOn = 1;
                switchedOnThisTick = true;
                g.startupActuatorDelay_s = kStartupInterval_s;
            }
        };

        // Вентиляторы запускаем раньше ТЭНов.
        stageActuator(ventLeftOn,  g.stagedVent1LeftOn);
        stageActuator(ventLeftOn,  g.stagedVent2LeftOn);
        stageActuator(ventRightOn, g.stagedVent1RightOn);
        stageActuator(ventRightOn, g.stagedVent2RightOn);
        stageActuator(ten1LeftOn,  g.stagedTen1LeftOn);
        stageActuator(ten2LeftOn,  g.stagedTen2LeftOn);
        stageActuator(ten1RightOn, g.stagedTen1RightOn);
        stageActuator(ten2RightOn, g.stagedTen2RightOn);

        // Почему: побочные эффекты (изменение общего Model::DFR) держим в одном месте для простого аудита.
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
 
     static void ControlStep1s()
     {
        // В рабочем автоматическом режиме клапан вытяжки держим закрытым.
        // Он открывается только в последовательности остановки.
        Model::DFR.Water_Flap = 1;

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
                ApplyOutputs(
                    /*ventLeftOn*/  0,
                    /*ventRightOn*/ 0,
                    /*ten1LeftOn*/  0,
                    /*ten2LeftOn*/  0,
                    /*ten1RightOn*/ 0,
                    /*ten2RightOn*/ 0,
                    /*injOn*/       0,
                    /*outOn*/       0);
                return;
            }
        }

       // В автоматическом режиме воротами управляет только GateControl.
       // Здесь принудительно снимаем команды, чтобы не оставалось "залипших" битов.
       GateControl_SetCommand(GateControlCommand::Open, 0);
       GateControl_SetCommand(GateControlCommand::Close, 0);
       GateControl_SetCommand(GateControlCommand::Deblock, 0);

         // Читаем последние значения из Model.
         const float T_supL_C = DeciToC((int16_t)Model::getCurrentVal_T(kSensSupLeft_T_H));
         const float RH_supL  = DeciToRH((int16_t)Model::getCurrentVal_H(kSensSupLeft_T_H));
         const float T_supR_C = DeciToC((int16_t)Model::getCurrentVal_T(kSensSupRight_T_H));
         const float RH_supR  = DeciToRH((int16_t)Model::getCurrentVal_H(kSensSupRight_T_H));
         const float T_ret_C  = DeciToC((int16_t)Model::getCurrentVal_T(kSensReturn_T_H));
         const float RH_ret   = DeciToRH((int16_t)Model::getCurrentVal_H(kSensReturn_T_H));
 
         // Почему: переменные оставлены под будущие связи (например, управление w по подаче),
         //         но сейчас подавляем предупреждения компилятора о неиспользуемых значениях.
         (void)RH_supL;
         (void)RH_supR;
 
         const float fish1_C = DeciToC((int16_t)Model::getCurrentVal_T(kSensFish1_T));
         const float fish2_C = DeciToC((int16_t)Model::getCurrentVal_T(kSensFish2_T));

         // Почему: проверяем, что датчик активен и используется в алгоритме разморозки.
         const bool fish1Enabled = (Sensor_array[kSensFish1_T].Active == 1) && (Sensor_array[kSensFish1_T].UseInDefrost != 0);
         const bool fish2Enabled = (Sensor_array[kSensFish2_T].Active == 1) && (Sensor_array[kSensFish2_T].UseInDefrost != 0);

         // Переменная haveFish_C является флагом, указывающим на наличие активных датчиков температуры продукта. 
         // Она принимает значения:
         // 0 - если нет активных датчиков температуры продукта (fish1 и fish2 отключены или не используются)
         // 1 - если хотя бы один датчик температуры продукта активен и используется в алгоритме
         float fishHot_C = 0.0f;
         float fishCold_C = 0.0f;
         float fishDelta_C = 0.0f;
         uint8_t haveFish_C = 0;

         if (fish1Enabled && fish2Enabled)
         {
             fishHot_C = (fish1_C >= fish2_C) ? fish1_C : fish2_C;
             fishCold_C = (fish1_C < fish2_C) ? fish1_C : fish2_C;
             fishDelta_C = fishHot_C - fishCold_C;
             haveFish_C = 1;
         }
         else if (fish1Enabled)
         {
             fishHot_C = fish1_C;
             fishCold_C = fish1_C;
             fishDelta_C = 0.0f;
             haveFish_C = 1;
         }
         else if (fish2Enabled)
         {
             fishHot_C = fish2_C;
             fishCold_C = fish2_C;
             fishDelta_C = 0.0f;
             haveFish_C = 1;
         }
         else
         {
             // Почему: без обратной связи по температуре в теле продукта алгоритм должен быть безопасным (без нагрева).
             // флаг отсутствия последней измеренной температуры продукта.
             // 0 - отсутствует последняя измеренная температура продукта.
             // 1 - присутствует последняя измеренная температура продукта.
             g.haveLastFishHot = 0;
         }

         // Почему: выбираем фазу разморозки на основе температуры самой холодной точки продукта.
         const Phase phase = (haveFish_C != 0) ? SelectPhase(fishCold_C) : Phase::WarmUp;
         const Limits lim = GetLimits(phase);
         const Targets tgt = GetTargets(phase);
 
         // Переводим уставку влажности в абсолютную влажность при температуре возврата.
         const float w_ret = HumidityRatio_kgkg(T_ret_C, RH_ret);
         const float w_ret_target = HumidityRatio_kgkg(T_ret_C, tgt.returnTargetRH_percent);
 
         // ─────────────────────────────────────────────────────────────────────────
         // Ограничитель безопасности: уменьшаем нагрев при приближении к ограничениям качества продукта.
         // Почему: ТЭНы мощные, продукт инерционный — проактивное ограничение предотвращает "перелёт".
         // ─────────────────────────────────────────────────────────────────────────
         // Переменная heatScale01 используется как ограничитель мощности нагрева для защиты качества продукта. 
         // Она представляет собой коэффициент (от 0.0 до 1.0), который умножается на расчётную мощность ТЭНов.
         float heatScale01 = 1.0f;

         // Если нет активных датчиков температуры продукта, то отключаем нагрев
         if (haveFish_C == 0)
         {
             heatScale01 = 0.0f;
         }
 
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
         if (g.haveLastFishHot != 0)
         {
             const float rate_Cps = (fishHot_C - g.lastFishHot_C) / kDt_s; // скорость прогрева "горячей" точки, град/сек
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
 
         // Ограничение по температуре подачи, чтобы не получить слишком горячие струи воздуха.
         if (T_supL_C > lim.supplyMax_C || T_supR_C > lim.supplyMax_C)
         {
             heatScale01 = 0.0f;
         }
 
         // ─────────────────────────────────────────────────────────────────────────
         // Регулирование температуры (общая составляющая + баланс лево/право).
         // Выход выражен в "эквивалентах ТЭНа": 0..2 на сторону (два ТЭНа на сторону).
         // Почему: так отображение на актуаторы явно и удобно строить скважность.
         // ─────────────────────────────────────────────────────────────────────────
         const float T_sup_avg_C = 0.5f * (T_supL_C + T_supR_C);
         const float eT_common = tgt.supplySet_C - T_sup_avg_C;
 
         // Базовый запрос мощности с учётом ограничителя.
         float uCommon_TEN = g.piSupplyCommon.Step(eT_common, kDt_s, 0.0f, 2.0f);
         uCommon_TEN *= heatScale01;
 
         // Балансировка лево/право по разности температур потоков подачи.
         const float eT_diff = (T_supL_C - T_supR_C); // цель: 0
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
        const uint16_t kTenMinHold_s = g_defrostParams.tenMinHold_s;
         const uint8_t ten1L_on = g.left.ten1Hold.Step(ten1L_desired, kTenMinHold_s);
         const uint8_t ten2L_on = g.left.ten2Hold.Step(ten2L_desired, kTenMinHold_s);
         const uint8_t ten1R_on = g.right.ten1Hold.Step(ten1R_desired, kTenMinHold_s);
         const uint8_t ten2R_on = g.right.ten2Hold.Step(ten2R_desired, kTenMinHold_s);
 
         // Вентиляторы: работаем непрерывно, пока алгоритм включен.
         // Почему: стабильный расход упрощает идентификацию контуров и защищает ТЭНы от локального перегрева.
         const uint8_t ventL_on = 1;
         const uint8_t ventR_on = 1;
 
         // ─────────────────────────────────────────────────────────────────────────
         // Управление влажностью (форсунка + вытяжка) по абсолютной влажности возврата.
         // Почему: абсолютная влажность лучше отражает "сушащее" действие воздуха, чем RH (RH зависит от T).
         // ─────────────────────────────────────────────────────────────────────────
         const float wErr = w_ret_target - w_ret;
 
         // Форсунка как медленный актуатор по скважности.
         // Почему: у форсунки есть инерция/перенос капель — модуляция скважностью мягче, чем жёсткий ON/OFF.
         float injDuty = 0.0f;
         if (wErr > g.wDeadband_kgkg)
         {
             injDuty = Clamp(g_defrostParams.injGain * (wErr - g.wDeadband_kgkg), 0.0f, 1.0f);
         }
 
        // Вытяжка: последовательное управление заслонкой и вентилятором для предотвращения частых переключений.
        // Почему: вытяжка влияет и на влажность, и на теплопотери; частые переключения раскачивают температуру.
        // Сначала полностью открывается заслонка, потом включается вентилятор.
        if (g.outHold_s > 0)
        {
            g.outHold_s--;
        }

        if (g.outHold_s == 0)
        {
            if (wErr < -g.wDeadband_kgkg)
            {
                // Нужно включить вытяжку
                if (g.outDamperState == 0)
                {
                    // Начинаем открытие заслонки
                    g.outDamperState = 1;      // состояние: открывается
                    g.outDamperTimer_s = g_defrostParams.outDamperTimer_s;
                    g.outFanOn = 0;            // вентилятор пока выключен
                    g.outHold_s = g_defrostParams.outHold_s;
                }
            }
            else if (wErr > 0.0f)
            {
                // Нужно выключить вытяжку
                g.outDamperState = 0;          // заслонка закрыта
                g.outDamperTimer_s = 0;        // сброс таймера
                g.outFanOn = 0;                // выключить вентилятор
                g.outHold_s = g_defrostParams.outHold_s;
            }
        }

        // Управление последовательностью открытия заслонки и включения вентилятора
        if (g.outDamperState == 1)
        {
            // Заслонка открывается
            if (g.outDamperTimer_s > 0)
            {
                g.outDamperTimer_s--;
            }
            else
            {
                // Заслонка открылась: ждём настраиваемую задержку перед включением вентилятора.
                g.outDamperState = 2;
                g.outDamperTimer_s = g_defrostParams.outFanDelay_s;
            }
        }
        else if (g.outDamperState == 2)
        {
            if (g.outDamperTimer_s > 0)
            {
                g.outDamperTimer_s--;
            }
            else
            {
                g.outDamperState = 3;
                g.outFanOn = 1;
            }
        }

        // Обновление общего состояния вытяжки (для совместимости с существующим кодом)
        g.outOn = g.outFanOn;
 
         // Предпочитаем взаимоисключение: если вытяжка включена, форсунку не используем.
         // Почему: совместная работа тратит энергию и делает управление плохо идентифицируемым.
         if (g.outOn != 0)
         {
             injDuty = 0.0f;
         }
 
         const uint8_t inj_desired = g.injPwm.Step(injDuty);
         // Почему: форсунка (клапан/насос) тоже не любит слишком частые переключения.
        const uint16_t kInjMinHold_s = g_defrostParams.injMinHold_s;
         const uint8_t inj_on = g.injHold.Step(inj_desired, kInjMinHold_s);
         const uint8_t out_on = g.outOn ? 1 : 0;
 
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
 } // namespace
 
 static void ShutdownSequence()
{
    // Безопасное выключение всех исполнительных механизмов при остановке алгоритма

    // ТЭНы: отключить все нагреватели
    Model::DFR.Ten1_Left = 0;
    Model::DFR.Ten2_Left = 0;
    Model::DFR.Ten1_Right = 0;
    Model::DFR.Ten2_Right = 0;

    // Вентиляторы: отключить все вентиляторы подачи воздуха
    Model::DFR.Vent1_Left = 0;
    Model::DFR.Vent2_Left = 0;
    Model::DFR.Vent1_Right = 0;
    Model::DFR.Vent2_Right = 0;

    // Форсунки: отключить увлажнение
    Model::DFR._Inj = 0;

    // Ворота: открываем через тот же алгоритм GateControl в текущем режиме.
    GateControl_SetCommand(GateControlCommand::Open, 1);
    g.shutdownGateOpening = 1;

    // Вытяжка: сначала открыть клапан, затем включить вентилятор на 5 минут.
    // Water_Flap: 1 = клапан закрыт, 0 = клапан открыт.
    Model::DFR.Water_Flap = 0;
    g.outDamperState = 1;             // клапан открывается
    g.outDamperTimer_s = g_defrostParams.outDamperTimer_s;
    g.outFanOn = 0;                   // до открытия клапана вентилятор выключен
    g.outOn = 0;
    Model::DFR._Out = 0;
    g.shutdownOutFanRemain_s = 300;   // 5 минут после открытия клапана
    g.shutdownActive = 1;
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
        DefrostControl_LoadParams();
         ResetState();
     }
 
     void DefrostControl_SetEnabled(uint8_t enabled)
     {
         // Почему: отдельный флаг enabled позволяет безопасно вводить алгоритм, не ломая существующую логику авто/ручного режима.
        const uint8_t newEnabled = enabled ? 1 : 0;

        if (newEnabled != 0)
        {
            ResetState();
           // Почему: запуск автоматического алгоритма должен выполняться именно в автоматическом режиме.
           // Иначе на реле будет отправляться ручной регистр, а счётчик runtime не будет увеличиваться.
           GateControl_SetManualMode(0);

           // При повторном старте во время post-shutdown вытяжки немедленно останавливаем
           // вентилятор вытяжки и закрываем заслонку, не дожидаясь следующего тика 1 Гц.
           g.shutdownActive = 0;
           g.shutdownGateOpening = 0;
           g.shutdownOutFanRemain_s = 0;
           g.outFanOn = 0;
           g.outOn = 0;
           g.outDamperState = 0;
           g.outDamperTimer_s = 0;
           Model::DFR._Out = 0;
           Model::DFR.Water_Flap = 1;

           // Сбрасываем команды ворот перед формированием команды на старт.
           GateControl_SetCommand(GateControlCommand::Open, 0);
           GateControl_SetCommand(GateControlCommand::Close, 0);
           GateControl_SetCommand(GateControlCommand::Deblock, 0);

          // Автоматический режим: зелёная лампа включена, красная выключена.
          ApplyModeLamps(LampModeState::AutoActive);

            // На старте авто-режима закрываем ворота, если они не в нижнем положении.
            if (GateControl_IsClosedPosition() == 0)
            {
                GateControl_SetCommand(GateControlCommand::Close, 1);
                g.startupGateClosing = 1;
            }
            else
            {
                g.startupGateClosing = 0;
                GateControl_SetCommand(GateControlCommand::Close, 0);
            }

            g.shutdownActive = 0;
            g.shutdownOutFanRemain_s = 0;
            g.enabled = 1;
            return;
        }

        g.enabled = 0;
        if (g.enabled == 0)
         {
             ShutdownSequence();  // Безопасное выключение всех элементов
            g.runtimeSeconds = 0; // Время работы алгоритма обнуляется при остановке
            // Останов: зелёная лампа выключена, красная включена.
            ApplyModeLamps(LampModeState::StoppedOrManual);
        }
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
        g_defrostParamsStorage.version = kDefrostParamsVersion;
        memcpy(&g_defrostParamsStorage.params, &g_defrostParams, sizeof(DefrostParams_t));
        g_defrostParamsStorage.payloadCrc = ParamsCrc16(
            (const uint8_t *)&g_defrostParamsStorage.params,
            sizeof(DefrostParams_t));
    }

    void DefrostControl_LoadParams(void)
    {
        const uint16_t crc = ParamsCrc16(
            (const uint8_t *)&g_defrostParamsStorage.params,
            sizeof(DefrostParams_t));

        if (g_defrostParamsStorage.version == kDefrostParamsVersion &&
            g_defrostParamsStorage.payloadCrc == crc)
        {
            memcpy(&g_defrostParams, &g_defrostParamsStorage.params, sizeof(DefrostParams_t));
        }
        else
        {
            LoadDefaultParams(&g_defrostParams);
            DefrostControl_SaveParams();
        }

        for (uint8_t i = 0; i < kDefrostSensorCount; ++i)
        {
            Sensor_array[i].UseInDefrost = (g_defrostParams.sensorUseInDefrost[i] != 0u) ? 1u : 0u;
        }
    }
 
     void DefrostControl_Update1s(void)
     {
         // Уважаем ручной режим и внешний флаг enabled.
         // Почему: ручной режим должен быть главным; алгоритм не должен "бороться" с оператором.
         if (g.enabled == 0)
         {
            // В режиме останова всегда показываем красную лампу.
            ApplyModeLamps(LampModeState::StoppedOrManual);

            if (g.shutdownActive != 0)
             {
                // Открытие ворот выполняем через API GateControl.
                if (g.shutdownGateOpening != 0)
                {
                    if (GateControl_IsCommandActive(GateControlCommand::Open) == 0)
                    {
                        g.shutdownGateOpening = 0;
                    }
                }
                else
                {
                    GateControl_SetCommand(GateControlCommand::Open, 0);
                    GateControl_SetCommand(GateControlCommand::Close, 0);
                    GateControl_SetCommand(GateControlCommand::Deblock, 0);
                }

                Model::DFR.Water_Flap = 0;

                if (g.outDamperState == 1)
                {
                    if (g.outDamperTimer_s > 0)
                    {
                        g.outDamperTimer_s--;
                    }
                    else
                    {
                        g.outDamperState = 2;
                        g.outFanOn = 1;
                        g.shutdownOutFanRemain_s = 300;
                    }
                }

                if (g.outFanOn != 0 && g.shutdownOutFanRemain_s > 0)
                {
                    g.shutdownOutFanRemain_s--;
                    if (g.shutdownOutFanRemain_s == 0)
                    {
                        g.outFanOn = 0;
                    }
                }

                Model::DFR._Out = g.outFanOn ? 1 : 0;

                if (g.outFanOn == 0 && g.outDamperState >= 2 && g.shutdownOutFanRemain_s == 0)
                 {
                    g.shutdownActive = 0;
                    GateControl_SetCommand(GateControlCommand::Open, 0);
                    GateControl_SetCommand(GateControlCommand::Close, 0);
                    GateControl_SetCommand(GateControlCommand::Deblock, 0);
                 }
             }
             return;
         }         

        // В ручном режиме автоматический алгоритм и счётчик времени не должны выполняться.
        if (GateControl_GetManualMode() != 0)
        {
            // В ручном режиме горит красная лампа.
            ApplyModeLamps(LampModeState::StoppedOrManual);
            return;
        }

        ControlStep1s();

        if (g.runtimeSeconds < UINT32_MAX)
        {
            g.runtimeSeconds++;
        }
     }
 }

