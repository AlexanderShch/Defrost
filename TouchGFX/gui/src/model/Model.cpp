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

//******************** Programming sensor **********************
void Model::setCurrentVal_PR(uint8_t SensNumber, uint8_t Val)
{
	Address_PR_sensor = SensNumber;
	BaudRate_PR_sensor = Val;
	FlagCurrentValue_PR_sensor = 1;
}

uint8_t Model::getCurrentAddress_PR(void)
{
	return Address_PR_sensor;
}

uint8_t Model::getCurrentBaudRate_PR(void)
{
	return BaudRate_PR_sensor;
}

void Model::clearFlagCurrentVal_PR_Chng(void)
{
	FlagCurrentValue_PR_sensor = 0;
}

int8_t Model::getFlagCurrentVal_PR_Chng(void)
{
	return FlagCurrentValue_PR_sensor;
}
//*****************************************************
void Model::ValUpdateModel()
{
	modelListener->ValUpdatePresenter();
}
//*****************************************************

// definition of static variable. Member function definitions belong in the scope where the class is defined.
int Model::CurrentValueT[SQ] = {0};
int8_t Model::FlagCurrentValueTChanged[SQ] = {0};
int Model::CurrentValueH[SQ] = {0};
int8_t Model::FlagCurrentValueHChanged[SQ] = {0};
uint8_t Model::Type_of_sensor = 0;
int Model::BaudRate_WR_to_sensor = 0;
uint8_t Model::Address_WR_to_sensor = 0;
uint8_t Model::Flag_WR_to_sensor = 0;
uint8_t Model::Flag_Alert = 0;
uint8_t Model::BaudRate_PR_sensor = 0;
uint8_t Model::Address_PR_sensor = 0;
uint8_t Model::FlagCurrentValue_PR_sensor = 0;

// определение переменных для корректировки
uint8_t Model::Index_CORR_sensor;			// индекс корректируемого датчика в массиве датчиков
uint8_t Model::Flag_CORR_ready = 0;			// флаг готовности корректировать датчик
int Model::T_CORR_sensor = 0;				// Текущая температура
int Model::H_CORR_sensor = 0;				// Текущая влажность (сопротивление для типа 2),
int Model::R_CORR_sensor = 0;				// Текущая влажность (сопротивление для типа 2),
uint8_t Model::Flag_Corr_T_changed = 0;		// флаг для обновления данных на экране
uint8_t Model::Flag_Corr_H_changed = 0;	// флаг для обновления данных на экране
uint8_t Model::Flag_Corr_R_changed = 0;	// флаг для обновления данных на экране
int16_t Model::CORR_T_sensor = 0;			// значение T для корректировки
int16_t Model::CORR_H_sensor = 0;			// значение H для корректировки
int16_t Model::CORR_R_sensor = 0;			// значение R для корректировки
uint8_t Model::Type_CORR_sensor = 0;
