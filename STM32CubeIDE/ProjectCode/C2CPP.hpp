/*
 * C2CPP.hpp
 *
 *  Created on: Aug 13, 2023
 *      Author: alsh1
 */

#ifndef C2CPP_HPP_
#define C2CPP_HPP_

void DataTimerFunc();
void DataFunc(void);
void ReadDataFunc();
void InitData();
void TX_ToServer();

#ifdef __cplusplus
extern "C" {
#endif
	void DataTimerFunc_C();
	void DataFunc_C(void);
	void ReadDataFunc_C();
	void InitDataVariables_C();
	void TransferToServer_С();
#ifdef __cplusplus
}
#endif




#endif /* C2CPP_HPP_ */
