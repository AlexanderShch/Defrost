#ifndef MODEL_HPP
#define MODEL_HPP

#define SensNum_DefrOperTemp 5	// defroster operating T
#define SensNum_ProdFinalTemp 6	// product final T

#include "Data.hpp"
#include "cmsis_os.h"

/*	Current Value from sensor saved in CurrentValue[SQ] array
 * 	0 - defroster T, H left
 * 	1 - defroster T, H right
 * 	2 - defroster T, H center
 *	3 - fish T left
 *	4 - fish T right
 * 	5 - defroster operating T
 * 	6 - product final T
 *
 */
class ModelListener;

// Регистр управления дефростером, битовый регистр, Имена битов
typedef struct
{
	unsigned Vent1_Left:1;		// 0 Вентилятор1 левый
	unsigned Vent2_Left:1;		// 1 Вентилятор2 левый
	unsigned Vent1_Right:1;	    // 2 Вентилятор1 правый
	unsigned Vent2_Right:1; 	// 3 Вентилятор2 правый
	unsigned Ten1_Left:1;		// 4 Тэн1 левый
	unsigned Ten2_Left:1;		// 5 Тэн2 левый
	unsigned Ten1_Right:1;		// 6 Тэн1 правый
	unsigned Ten2_Right:1;		// 7 Тэн2 правый
	unsigned _Out:1;     		// 8 Вытяжной вентилятор включить
	unsigned _Inj:1;     		// 9 Водяную форсунку включить
	unsigned Water_Flap:1;		// 10 Закрыть защитную заслонку вытяжного вентилятора
	unsigned Gate_Up:1;			// 11 Поднять ворота
	unsigned Gate_Dbl:1;		// 12 Разблокировать управление воротами
	unsigned Gate_Down:1;		// 13 Опустить ворота
	unsigned _Wrk:1;     		// 14 Включить зелёную лампу РАБОТА
	unsigned _Stp:1;     		// 15 Включить красную лампу СТОП

	// наименования и порядок переменных в программе сервера
//	unsigned _V0:1; 			// Циркуляционный вентилятор 1 левый включить
//	unsigned _V1:1; 			// Циркуляционный вентилятор 2 левый включить
//	unsigned _V2:1; 			// Циркуляционный вентилятор 1 правый включить
//	unsigned _V3:1; 			// Циркуляционный вентилятор 2 правый включить
//	unsigned _H0:1; 			// Нагреватель (ТЭН) 1 левый включить
//	unsigned _H1:1; 			// Нагреватель (ТЭН) 2 левый включить
//	unsigned _H2:1; 			// Нагреватель (ТЭН) 1 правый включить
//	unsigned _H3:1; 			// Нагреватель (ТЭН) 2 правый включить
//	unsigned _Out:1;     		// Вытяжной вентилятор включить
//	unsigned _Inj:1;     		// Водяную форсунку включить
//	unsigned _Flp:1;     		// Закрыть защитную заслонку вытяжного вентилятора
//	unsigned _Opn:1;     		// Ворота открыть
//	unsigned _Stp:1;     		// Ворота остановить
//	unsigned _Cls:1;     		// Ворота закрыть
//	unsigned _Snd:1;     		// Звуковой сигнал включить
//	unsigned _Wrk:1;     		// Включить зелёную лампу РАБОТА


} DFR_REGISTERS_t;

// Регистр входных сигналов дефростера, считанных с модуля ввода-вывода.
typedef union
{
	uint16_t Raw;
	struct
	{
		unsigned DI0:1;
		unsigned DI1:1;
		unsigned DI2:1;
		unsigned DI3:1;
		unsigned DI4:1;
		unsigned DI5:1;
		unsigned DI6:1;
		unsigned DI7:1;
		unsigned DI8:1;
		unsigned DI9:1;
		unsigned DI10:1;
		unsigned DI11:1;
		unsigned Gate_Close:1; // бит 12: ворота закрыты
		unsigned Gate_Open:1;  // бит 13: ворота открыты
		unsigned DI14:1;
		unsigned DI15:1;
	} Bits;
} DI_DFR_REGISTERS_t;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();
    void ValUpdateModel();
    
    // Получение версии прошивки
    static const char* getFirmwareVersion();

    static void setCurrentVal_T(int8_t SensNumber, short Val);
    static short getCurrentVal_T(int8_t SensNumber);
    static void setCurrentVal_H(int8_t SensNumber, short Val);
    static short getCurrentVal_H(int8_t SensNumber);
    static void setCurrentVal_PR(uint8_t SensNumber, uint8_t Val);		// установка текущих значений адреса и скорости программируемого датчика
    static uint8_t getCurrentAddress_PR(void);							// получить текущее значение адреса программируемого датчика
    static uint8_t getCurrentBaudRate_PR(void);							// получить текущее значение скорости программируемого датчика

    static void clearFlagCurrentVal_T_Chng(int8_t SensNumber);
    static int8_t getFlagCurrentVal_T_Chng(int8_t SensNumber);
    static void clearFlagCurrentVal_H_Chng(int8_t SensNumber);
    static int8_t getFlagCurrentVal_H_Chng(int8_t SensNumber);
    static void clearFlagCurrentVal_PR_Chng();							// очистить флаг изменения значения адреса и скорости программируемого датчика
    static int8_t getFlagCurrentVal_PR_Chng();							// получить флаг изменения значения адреса и скорости программируемого датчика

    static void setDefrostManualGroupEnabled(uint8_t groupIndex1to4, bool enabled);
    static void setDefrostManualModeEnabled(bool enabled);
    static bool isDefrostManualModeEnabled();
    static bool isDefrostManualGroupEnabled(uint8_t groupIndex1to4);

    // Состояние ворот от модуля ввода-вывода (входные сигналы)
    static uint8_t Gate_Open;   // бит 13: ворота открыты
    static uint8_t Gate_Close;  // бит 12: ворота закрыты
    static uint8_t Gate_Alarm;  // флаг аварийного режима ворот
    static uint8_t Gate_PosTop;    // флаг конечного положения "ворота вверху" (для аварийного режима)
    static uint8_t Gate_PosBottom; // флаг конечного положения "ворота внизу" (для аварийного режима)

    // программирование
    static int BaudRate_WR_to_sensor;		// скорость для записи в датчик
    static uint8_t Type_of_sensor;			// установленное значение типа датчика
    static uint8_t Address_WR_to_sensor;	// адрес для записи в датчик
    static uint8_t Flag_WR_to_sensor;		// флаг готовности данных для записи в датчик
    static uint8_t Flag_Alert;				// флаг всплывающего окна предупреждения
    // корректировка
    static uint8_t Index_CORR_sensor;		// индекс корректируемого датчика в массиве датчиков
    static uint8_t Flag_CORR_ready;			// флаг готовности к корректировке датчика
    static uint8_t Type_CORR_sensor;		// тип корректируемого датчика
    static int T_CORR_sensor;				// Текущая температура
    static int H_CORR_sensor;				// Текущая влажность (сопротивление для типа 2),
    static int R_CORR_sensor;				// Текущая влажность (сопротивление для типа 2),
    static uint8_t Flag_Corr_T_changed;		// флаг для обновления данных на экране
    static uint8_t Flag_Corr_H_changed;		// флаг для обновления данных на экране
    static uint8_t Flag_Corr_R_changed;		// флаг для обновления данных на экране
    static int16_t CORR_T_sensor;			// значение T для корректировки
    static int16_t CORR_H_sensor;			// значение H для корректировки
    static int16_t CORR_R_sensor;			// значение R для корректировки
    // регистры управления устройствами
    static DI_DFR_REGISTERS_t DI_DFR;		// Регистр входных сигналов дефростера
    static DFR_REGISTERS_t DFR;				// Объявление регистра состояния управления устройствами
    static DFR_REGISTERS_t DFR_current;		// Объявление регистра текущего отображения состояния управления устройствами
    static DFR_REGISTERS_t DFR_chng_flag;	// Объявление регистра флагов изменения состояния управления устройствами
    static DFR_REGISTERS_t DFR_manual;		// Объявление регистра флагов ручного управления устройствами
    static uint8_t Flag_DFR_manual;			// флаг включения ручного режима управления

protected:
    ModelListener* modelListener;
    static short CurrentValueT[SQ];				// текущее значение Т на экране
    static short FilteredValueT[SQ];			// отфильтрованное и усреднённое значение Т
    static int8_t FlagCurrentValueTChanged[SQ];	// флаг изменения Т
    static short CurrentValueH[SQ];				// текущее значение Н на экране
    static int8_t FlagCurrentValueHChanged[SQ];	// флаг изменения Н
    static uint8_t BaudRate_PR_sensor;			// скорость программируемого датчика
    static uint8_t Address_PR_sensor;			// адрес программируемого датчика
    static uint8_t FlagCurrentValue_PR_sensor;	// флаг изменения текущих значений программирования Т и Н для вывода на экран

    static uint8_t DefrostManualGroupMask;      // биты 0..3 => группы 1..4
    static uint8_t DefrostManualGlobalEnabled;  // переключатель ручного режима (BTN_Manual)
};

#endif // MODEL_HPP
