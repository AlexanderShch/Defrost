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

    static int BaudRate_WR_to_sensor;		// скорость для записи в датчик
    static uint8_t Type_of_sensor;			// установленное значение типа датчика
    static uint8_t Address_WR_to_sensor;	// адрес для записи в датчик
    static uint8_t Flag_WR_to_sensor;		// флаг готовности данных для записи в датчик

protected:
    ModelListener* modelListener;
    static int CurrentValueT[SQ];
    static int8_t FlagCurrentValueTChanged[SQ];
    static int CurrentValueH[SQ];
    static int8_t FlagCurrentValueHChanged[SQ];
    static uint8_t BaudRate_PR_sensor;
    static uint8_t Address_PR_sensor;
    static uint8_t FlagCurrentValue_PR_sensor;

};

#endif // MODEL_HPP
