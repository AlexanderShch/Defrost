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

#ifdef __cplusplus
extern "C" {
#endif
	void DataTimerFunc_C();
	void DataFunc_C(void);
	void ReadDataFunc_C();
	void InitDataVariables_C();
#ifdef __cplusplus
}
#endif




#endif /* C2CPP_HPP_ */
