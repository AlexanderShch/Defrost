/*
 * Data.hpp
 *
 *  Created on: Jul 3, 2023
 *      Author: gdr
 */

#ifndef DATA_HPP_
#define DATA_HPP_

#include "cmsis_os.h"
#include "ModBus.hpp"

#define TQ 16				// time quantity for saving measures in array
#define SQ 7				// датчики TH дефростера (0-2) + датчики продукта (3, 4) + T корпуса (5) + MB_IO (6)
#define STQ 5				// sensors type quantity - кол-во типов датчиков и модулей IO
#define FLAG_ReadData 1ul	// read data event flag 0x00000001ul
#define DataRead_ShiftCounter 1 // Кол-во 1 сек периодов между считываниями данных

class Sensor
{
public:
	Sensor(){};										// declare default constructor
	Sensor(unsigned int Time, int T, int H){};		// declare constructor
	static void PutData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param, int Val);
	static int GetData(unsigned int TimeFromStart, unsigned char SensNum, unsigned char Param);

protected:
	static unsigned int Time[TQ][SQ];	// number of time quantum measuring
	static int T[TQ][SQ];			// temperature
	static int H[TQ][SQ];			// humidity
};

#endif /* DATA_HPP_ */

