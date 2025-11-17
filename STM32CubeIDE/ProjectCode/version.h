/*
 * version.h
 *
 *  Created on: October 25, 2025
 *      Author: System
 *  Description: Версия прошивки устройства
 */

#ifndef VERSION_H_
#define VERSION_H_

// Версия прошивки (Semantic Versioning: MAJOR.MINOR.PATCH)
#define FW_VERSION_MAJOR    1   // несовместимые изменения API
#define FW_VERSION_MINOR    1   // новая функциональность, обратно совместимая
#define FW_VERSION_PATCH    1   // исправления багов, обратно совместимые

// Строковое представление версии
#define FW_VERSION_STRING   "1.1.1"		// корректировать вручную, вместе с минор, мажор и патч

// Дата сборки (автоматически подставляется компилятором)
#define FW_BUILD_DATE       __DATE__
#define FW_BUILD_TIME       __TIME__

// Полная информация о версии
#define FW_VERSION_FULL     "Defrost Controller v" FW_VERSION_STRING " (" __DATE__ " " __TIME__ ")"

// Целочисленное представление версии (для сравнения)
// Формат: 0xMMmmpppp (MM - major, mm - minor, pppp - patch)
#define FW_VERSION_NUMBER   ((FW_VERSION_MAJOR << 24) | (FW_VERSION_MINOR << 16) | FW_VERSION_PATCH)

#endif /* VERSION_H_ */

