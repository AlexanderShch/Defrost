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

    static void clearFlagCurrentVal_T_Chng(int8_t SensNumber);
    static int8_t getFlagCurrentVal_T_Chng(int8_t SensNumber);
    static void clearFlagCurrentVal_H_Chng(int8_t SensNumber);
    static int8_t getFlagCurrentVal_H_Chng(int8_t SensNumber);

protected:
    ModelListener* modelListener;
    static int CurrentValueT[SQ];
    static int8_t FlagCurrentValueTChanged[SQ];
    static int CurrentValueH[SQ];
    static int8_t FlagCurrentValueHChanged[SQ];
};

#endif // MODEL_HPP
