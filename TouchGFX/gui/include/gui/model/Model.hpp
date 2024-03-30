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

// ФлагиСостояния, битовый регистр, Имена битов
typedef struct
{
	unsigned Ten1_Left:1;		// Тэн1 левый
	unsigned Ten2_Left:1;		// Тэн2 левый
	unsigned Ten1_Right:1;		// Тэн1 правый
	unsigned Ten2_Right:1;		// Тэн2 правый
	unsigned Vent1_Left:1;		// Вентилятор1 левый
	unsigned Vent2_Left:1;		// Вентилятор2 левый
	unsigned Vent1_Right:1;	    // Вентилятор1 правый
	unsigned Vent2_Right:1; 	// Вентилятор2 правый
	unsigned Water_Flap:1;		// Водный клапан

} DFR_REGISTERS_t;

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

    static void setCurrentVal_T(int8_t SensNumber, int Val);
    static int getCurrentVal_T(int8_t SensNumber);
    static void setCurrentVal_H(int8_t SensNumber, int Val);
    static int getCurrentVal_H(int8_t SensNumber);
    static void setCurrentVal_PR(uint8_t SensNumber, uint8_t Val);		// установка текущих значений адреса и скорости программируемого датчика
    static uint8_t getCurrentAddress_PR(void);							// получить текущее значение адреса программируемого датчика
    static uint8_t getCurrentBaudRate_PR(void);							// получить текущее значение скорости программируемого датчика

    static void clearFlagCurrentVal_T_Chng(int8_t SensNumber);
    static int8_t getFlagCurrentVal_T_Chng(int8_t SensNumber);
    static void clearFlagCurrentVal_H_Chng(int8_t SensNumber);
    static int8_t getFlagCurrentVal_H_Chng(int8_t SensNumber);
    static void clearFlagCurrentVal_PR_Chng();							// очистить флаг изменения значения адреса и скорости программируемого датчика
    static int8_t getFlagCurrentVal_PR_Chng();							// получить флаг изменения значения адреса и скорости программируемого датчика

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
    static DFR_REGISTERS_t DFR;				// Объявление регистра состояния управления устройствами
    static DFR_REGISTERS_t DFR_current;		// Объявление регистра текущего отображения состояния управления устройствами
    static DFR_REGISTERS_t DFR_chng_flag;	// Объявление регистра флагов изменения состояния управления устройствами


protected:
    ModelListener* modelListener;
    static int CurrentValueT[SQ];				// текущее значение Т на экране
    static int8_t FlagCurrentValueTChanged[SQ];	// флаг изменения Т
    static int CurrentValueH[SQ];				// текущее значение Н на экране
    static int8_t FlagCurrentValueHChanged[SQ];	// флаг изменения Н
    static uint8_t BaudRate_PR_sensor;			// скорость программируемого датчика
    static uint8_t Address_PR_sensor;			// адрес программируемого датчика
    static uint8_t FlagCurrentValue_PR_sensor;	// флаг изменения текущих значений программирования Т и Н для вывода на экран
};

#endif // MODEL_HPP
