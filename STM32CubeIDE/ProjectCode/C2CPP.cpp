/*
 * C2CPP.cpp
 *
 *  Создан: Aug 13, 2023
 *  Автор: alsh1
 */

#include "C2CPP.hpp"
#include "CommandReceiver.hpp"

extern "C"
{
	void DataTimerFunc_C()	// запуск из osTimer с периодом 1 сек
	{
		DataTimerFunc();
	}
	void ReadDataFunc_C()	// запуск ReadData после таймера
	{
		ReadDataFunc();
	}
	void DataFunc_C()		// запуск DataAnalysis
	{
		DataFunc();
	}
	void InitDataVariables_C()
	{
		InitData();
	}
	void TransferToServer_С()
	{
		TX_ToServer();
	}

}






