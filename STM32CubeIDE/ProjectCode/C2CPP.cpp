/*
 * C2CPP.cpp
 *
 *  Created on: Aug 13, 2023
 *      Author: alsh1
 */

#include "C2CPP.hpp"
#include "CommandReceiver.hpp"

extern "C"
{
	void DataTimerFunc_C()	// start from osTimer 1 sec
	{
		DataTimerFunc();
	}
	void ReadDataFunc_C()	// start ReadData after timer
	{
		ReadDataFunc();
	}
	void DataFunc_C()		// start DataAnalysis
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
	void CommandReceiver_Task_C(void *argument)
	{
		CommandReceiver_Task(argument);
	}

}






