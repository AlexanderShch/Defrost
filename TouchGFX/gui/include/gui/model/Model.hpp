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

    static void setCurrentVal(int8_t SensNumber, int Val);
    static int getCurrentVal(int8_t SensNumber);

    static void clearFlagCurrentValChng(int8_t SensNumber);
    static int8_t getFlagCurrentValChng(int8_t SensNumber);

protected:
    ModelListener* modelListener;
    static int CurrentValue[SQ];
    static int8_t FlagCurrentValueChanged[SQ];
};

#endif // MODEL_HPP
