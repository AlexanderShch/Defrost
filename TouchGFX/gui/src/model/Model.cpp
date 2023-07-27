#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>


Model::Model() : modelListener(0)
{

}

void Model::tick()
{
	ValUpdateModel();

}

void Model::setCurrentVal(int8_t SensNumber, float Val)
{
	CurrentValue[SensNumber] = Val;
	FlagCurrentValueChanged[SensNumber] = 1;
}

float Model::getCurrentVal(int8_t SensNumber)
{
	return CurrentValue[SensNumber];
}

void Model::clearFlagCurrentValChng(int8_t SensNumber)
{
	FlagCurrentValueChanged[SensNumber] = 0;
}

int8_t Model::getFlagCurrentValChng(int8_t SensNumber)
{
	return FlagCurrentValueChanged[SensNumber];
}

void Model::ValUpdateModel()
{
	modelListener->ValUpdatePresenter();
}

// definition of static variable. Member function definitions belong in the scope where the class is defined.
float Model::CurrentValue[SQ] = {0};
int8_t Model::FlagCurrentValueChanged[SQ] = {0};
