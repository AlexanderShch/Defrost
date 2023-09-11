#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>


Model::Model() : modelListener(0)
{

}

void Model::tick()
{
	ValUpdateModel();

}

//******************** Temperature **********************
void Model::setCurrentVal_T(int8_t SensNumber, int Val)
{
	CurrentValueT[SensNumber] = Val;
	FlagCurrentValueTChanged[SensNumber] = 1;
}

int Model::getCurrentVal_T(int8_t SensNumber)
{
	return CurrentValueT[SensNumber];
}

void Model::clearFlagCurrentVal_T_Chng(int8_t SensNumber)
{
	FlagCurrentValueTChanged[SensNumber] = 0;
}

int8_t Model::getFlagCurrentVal_T_Chng(int8_t SensNumber)
{
	return FlagCurrentValueTChanged[SensNumber];
}

//******************** Humidity **********************
void Model::setCurrentVal_H(int8_t SensNumber, int Val)
{
	CurrentValueH[SensNumber] = Val;
	FlagCurrentValueHChanged[SensNumber] = 1;
}

int Model::getCurrentVal_H(int8_t SensNumber)
{
	return CurrentValueH[SensNumber];
}

void Model::clearFlagCurrentVal_H_Chng(int8_t SensNumber)
{
	FlagCurrentValueHChanged[SensNumber] = 0;
}

int8_t Model::getFlagCurrentVal_H_Chng(int8_t SensNumber)
{
	return FlagCurrentValueHChanged[SensNumber];
}
//*****************************************************

void Model::ValUpdateModel()
{
	modelListener->ValUpdatePresenter();
}

// definition of static variable. Member function definitions belong in the scope where the class is defined.
int Model::CurrentValueT[SQ] = {0};
int8_t Model::FlagCurrentValueTChanged[SQ] = {0};
int Model::CurrentValueH[SQ] = {0};
int8_t Model::FlagCurrentValueHChanged[SQ] = {0};
