#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include "version.h"

Model::Model() : modelListener(0)
{

}

void Model::tick()
{
	ValUpdateModel();

}

//******************** Temperature **********************
void Model::setCurrentVal_T(int8_t SensNumber, short Val)
{
	CurrentValueT[SensNumber] = Val;
	FlagCurrentValueTChanged[SensNumber] = 1;
}

short Model::getCurrentVal_T(int8_t SensNumber)
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
void Model::setCurrentVal_H(int8_t SensNumber, short Val)
{
	CurrentValueH[SensNumber] = Val;
	FlagCurrentValueHChanged[SensNumber] = 1;
}

short Model::getCurrentVal_H(int8_t SensNumber)
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

void Model::setDefrostManualGroupEnabled(uint8_t groupIndex1to4, bool enabled)
{
	if (groupIndex1to4 < 1 || groupIndex1to4 > 4)
	{
		return;
	}

	const uint8_t bit = static_cast<uint8_t>(1u << (groupIndex1to4 - 1));
	if (enabled)
	{
		DefrostManualGroupMask |= bit;
	}
	else
	{
		DefrostManualGroupMask &= static_cast<uint8_t>(~bit);
	}

	const bool manualModeEnabled = (DefrostManualGlobalEnabled != 0) || (DefrostManualGroupMask != 0);
	Flag_DFR_manual = manualModeEnabled ? 1 : 0;

	if (!manualModeEnabled)
	{
		// Почему: чтобы при повторном входе в ручной режим не применились "залипшие" ручные выходы.
		uint16_t* pDFRManual = (uint16_t*)&DFR_manual;
		*pDFRManual = 0;
	}
}

void Model::setDefrostManualModeEnabled(bool enabled)
{
	DefrostManualGlobalEnabled = enabled ? 1 : 0;

	// Если ручной режим выключен глобально, выключаем и все группы.
	if (!enabled)
	{
		DefrostManualGroupMask = 0;
		Gate_Alarm = 0;
		Gate_PosTop = 0;
		Gate_PosBottom = 0;
	}

	const bool manualModeEnabled = (DefrostManualGlobalEnabled != 0) || (DefrostManualGroupMask != 0);
	Flag_DFR_manual = manualModeEnabled ? 1 : 0;

	if (!manualModeEnabled)
	{
		uint16_t* pDFRManual = (uint16_t*)&DFR_manual;
		*pDFRManual = 0;
		Gate_Alarm = 0;
		Gate_PosTop = 0;
		Gate_PosBottom = 0;
	}
}

bool Model::isDefrostManualModeEnabled()
{
	return Flag_DFR_manual != 0;
}

bool Model::isDefrostManualGroupEnabled(uint8_t groupIndex1to4)
{
	if (groupIndex1to4 < 1 || groupIndex1to4 > 4)
	{
		return false;
	}

	const uint8_t bit = static_cast<uint8_t>(1u << (groupIndex1to4 - 1));
	return (DefrostManualGroupMask & bit) != 0;
}

/*
 * Функция: getFirmwareVersion
 * Описание: Возвращает строку с версией прошивки
 * Возвращает: указатель на строку FW_VERSION_STRING из version.h
 */
const char* Model::getFirmwareVersion()
{
	return FW_VERSION_STRING;
}
//*****************************************************

// definition of static variable. Member function definitions belong in the scope where the class is defined.
short Model::CurrentValueT[SQ] = {0};
int8_t Model::FlagCurrentValueTChanged[SQ] = {0};
short Model::CurrentValueH[SQ] = {0};
int8_t Model::FlagCurrentValueHChanged[SQ] = {0};
uint8_t Model::Type_of_sensor = 0;
int Model::BaudRate_WR_to_sensor = 0;
uint8_t Model::Address_WR_to_sensor = 0;
uint8_t Model::Flag_WR_to_sensor = 0;
uint8_t Model::Flag_Alert = 0;
uint8_t Model::Gate_Open = 0;
uint8_t Model::Gate_Close = 0;
uint8_t Model::Gate_Alarm = 0;
uint8_t Model::Gate_PosTop = 0;
uint8_t Model::Gate_PosBottom = 0;
uint8_t Model::BaudRate_PR_sensor = 0;
uint8_t Model::Address_PR_sensor = 0;
uint8_t Model::FlagCurrentValue_PR_sensor = 0;
uint8_t Model::DefrostManualGroupMask = 0;
uint8_t Model::DefrostManualGlobalEnabled = 0;
uint8_t Model::Flag_DFR_manual = 0;
DI_DFR_REGISTERS_t Model::DI_DFR = {};
DFR_REGISTERS_t Model::DFR;				// Регистр состояния управления устройствами
DFR_REGISTERS_t Model::DFR_current;		// Регистр текущего отображения состояния управления устройствами
DFR_REGISTERS_t Model::DFR_chng_flag;	// Регистр флагов изменения состояния управления устройствами
DFR_REGISTERS_t Model::DFR_manual;		// Регистр ручного управления устройствами

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



