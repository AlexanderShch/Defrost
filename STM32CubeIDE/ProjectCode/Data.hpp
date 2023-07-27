/*
 * Data.hpp
 *
 *  Created on: Jul 3, 2023
 *      Author: gdr
 */

#ifndef DATA_HPP_
#define DATA_HPP_


#define TQ 16				// time quantity for saving measures in array
#define SQ 9				// sensors quantity for measures
#define FLAG_ReadData 1ul		// read data event flag 0x00000001ul

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

void DataTimerFunc();
void DataFunc(void);
void ReadDataFunc();
void InitData();


#endif /* DATA_HPP_ */

