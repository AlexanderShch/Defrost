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
 
 #include "Data.hpp"                 // индексы датчиков SQ
 #include <gui\model\model.hpp>      // Model::getCurrentVal_* и битовый регистр Model::DFR
 #include "ModBus.hpp"
 
 namespace
 {
     extern SENSOR_typedef_t Sensor_array[SQ];

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
         // Почему: консервативные значения снижают риск агрессивного нагрева около/выше 0°C, где чаще портится поверхность.
         switch (p)
         {
             case Phase::WarmUp:
                 return Limits{
                    /*fishHotMax_C*/       20.0f,
                     /*fishHotRateMax_Cps*/ 0.020f,  // 1.2°C/min
                     /*fishDeltaMax_C*/     6.0f,
                     /*supplyMax_C*/        35.0f,
                 };
             case Phase::Plateau:
                 return Limits{
                    /*fishHotMax_C*/       20.0f,
                     /*fishHotRateMax_Cps*/ 0.015f,  // 0.9°C/min
                     /*fishDeltaMax_C*/     5.0f,
                     /*supplyMax_C*/        30.0f,
                 };
             case Phase::Finish:
             default:
                 return Limits{
                    /*fishHotMax_C*/       20.0f,
                     /*fishHotRateMax_Cps*/ 0.010f,  // 0.6°C/min
                     /*fishDeltaMax_C*/     4.0f,
                     /*supplyMax_C*/        26.0f,
                 };
         }
     }
 
     static Targets GetTargets(Phase p)
     {
         // Почему: повышенная влажность около 0°C снижает пересушивание и может улучшать теплопередачу.
         switch (p)
         {
             case Phase::WarmUp:
                 return Targets{
                     /*supplySet_C*/            30.0f,
                     /*returnTargetRH_percent*/ 85.0f,
                 };
             case Phase::Plateau:
                 return Targets{
                     /*supplySet_C*/            26.0f,
                     /*returnTargetRH_percent*/ 92.0f,
                 };
             case Phase::Finish:
             default:
                 return Targets{
                     /*supplySet_C*/            22.0f,
                     /*returnTargetRH_percent*/ 85.0f,
                 };
         }
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
         g.piSupplyCommon = PI{ /*kp*/ 0.18f, /*ki*/ 0.02f, /*i*/ 0.0f }; // стартовые; настройка обязательна
         g.leftRightTrimGain = 0.08f;   // коэффициент балансировки лево/право по разности температур потоков подачи.
         g.wDeadband_kgkg = 0.0008f;    // мёртвая зона для влажности.
         g.injPwm.Reset();              // сброс ШИМ-памяти для форсунки.
         g.injHold.Reset(0);            // сброс времени удержания форсунки.
        g.outOn = 0;                   // флаг отключения вытяжки.
        g.outHold_s = 0;               // время удержания вытяжки.
        g.outDamperState = 0;          // заслонка закрыта.
        g.outDamperTimer_s = 0;        // таймер открытия заслонки.
        g.outFanDelay_s = 5;           // задержка включения вентилятора.
        g.outFanOn = 0;                // вентилятор вытяжки выключен.
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
         // Почему: побочные эффекты (изменение общего Model::DFR) держим в одном месте для простого аудита.
         Model::DFR.Vent1_Left  = ventLeftOn ? 1 : 0;
         Model::DFR.Vent2_Left  = ventLeftOn ? 1 : 0;
         Model::DFR.Vent1_Right = ventRightOn ? 1 : 0;
         Model::DFR.Vent2_Right = ventRightOn ? 1 : 0;
 
         Model::DFR.Ten1_Left   = ten1LeftOn ? 1 : 0;
         Model::DFR.Ten2_Left   = ten2LeftOn ? 1 : 0;
         Model::DFR.Ten1_Right  = ten1RightOn ? 1 : 0;
         Model::DFR.Ten2_Right  = ten2RightOn ? 1 : 0;
 
         Model::DFR._Inj        = injOn ? 1 : 0;
         Model::DFR._Out        = outOn ? 1 : 0;
     }
 
     static void ControlStep1s()
     {
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
         trim_TEN = Clamp(trim_TEN, -0.6f, 0.6f);
 
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
         const uint16_t kTenMinHold_s = 10;
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
             const float k = 900.0f; // скважность на (кг/кг); стартовое значение, требует настройки
             injDuty = Clamp(k * (wErr - g.wDeadband_kgkg), 0.0f, 1.0f);
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
                    g.outDamperTimer_s = 10;   // время полного открытия заслонки (настраивается)
                    g.outFanOn = 0;            // вентилятор пока выключен
                    g.outHold_s = 15;          // общее время удержания
                }
            }
            else if (wErr > 0.0f)
            {
                // Нужно выключить вытяжку
                g.outDamperState = 0;          // заслонка закрыта
                g.outDamperTimer_s = 0;        // сброс таймера
                g.outFanOn = 0;                // выключить вентилятор
                g.outHold_s = 15;              // время удержания в выключенном состоянии
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
                // Заслонка полностью открыта, можно включать вентилятор
                g.outDamperState = 2;  // состояние: открыта
                g.outFanOn = 1;        // включить вентилятор вытяжки
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
         const uint16_t kInjMinHold_s = 5;
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
 
 extern "C"
 {
     void DefrostControl_Init(void)
     {
         ResetState();
     }
 
     void DefrostControl_SetEnabled(uint8_t enabled)
     {
         // Почему: отдельный флаг enabled позволяет безопасно вводить алгоритм, не ломая существующую логику авто/ручного режима.
         g.enabled = enabled ? 1 : 0;
         if (g.enabled == 0)
         {
             ResetState();
             // При выключении алгоритма выходы не трогаем: внешний код может захотеть оставить ручное управление как есть.
         }
     }
 
     void DefrostControl_Update1s(void)
     {
         // Уважаем ручной режим и внешний флаг enabled.
         // Почему: ручной режим должен быть главным; алгоритм не должен "бороться" с оператором.
         if (g.enabled == 0)
         {
             return;
         }
         if (Model::Flag_DFR_manual != 0)
         {
             return;
         }
 
         ControlStep1s();
     }
 }

