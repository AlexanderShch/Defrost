#ifndef MODEL_HPP
#define MODEL_HPP
#include <Data.hpp>
#include "cmsis_os.h"

/*	Current Value from sensor saved in CurrentValue[SQ] array
 * 	0 - defroster left
 * 	1 - defroster right
 * 	2 - defroster center
 *	3 - fish left
 *	4 - fish right
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
    static void setCurrentVal(int8_t SensNumber, float Val);
    static float getCurrentVal(int8_t SensNumber);
    static void clearFlagCurrentValChng(int8_t SensNumber);
    static int8_t getFlagCurrentValChng(int8_t SensNumber);

protected:
    ModelListener* modelListener;
    static float CurrentValue[SQ];
    static int8_t FlagCurrentValueChanged[SQ];
};

#endif // MODEL_HPP
