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
#define FW_VERSION_MINOR    3   // новая функциональность, обратно совместимая
#define FW_VERSION_PATCH    1   // исправления багов, обратно совместимые

// String representation is derived from numeric parts to avoid manual mismatches.
#define FW_VERSION_STRINGIFY_IMPL(x)  #x
#define FW_VERSION_STRINGIFY(x)       FW_VERSION_STRINGIFY_IMPL(x)
#define FW_VERSION_STRING             FW_VERSION_STRINGIFY(FW_VERSION_MAJOR) "." FW_VERSION_STRINGIFY(FW_VERSION_MINOR) "." FW_VERSION_STRINGIFY(FW_VERSION_PATCH)

// Дата сборки (автоматически подставляется компилятором)
#define FW_BUILD_DATE       __DATE__
#define FW_BUILD_TIME       __TIME__

// Полная информация о версии
#define FW_VERSION_FULL     "Defrost Controller v" FW_VERSION_STRING " (" __DATE__ " " __TIME__ ")"

// Целочисленное представление версии (для сравнения)
// Формат: 0xMMmmpppp (MM - major, mm - minor, pppp - patch)
#define FW_VERSION_NUMBER   ((FW_VERSION_MAJOR << 24) | (FW_VERSION_MINOR << 16) | FW_VERSION_PATCH)

#endif /* VERSION_H_ */

/*
v 1.3.1 (Dec 20 2025 14:30:00)
- добавлена обработка команды CFG_CMD_SET_INTERVAL (02 03) для установки интервала отправки телеметрии на сервер.

v 1.3.0 (Dec 18 2025 14:30:00)
- изменена структура кадра данных, поступающих от контроллера. Новый формат без маркера конца и содержит длину кадра: 
AA 55 + Type + Len + Data[Len] + CRC16, где CRC считается по Type+Len+Data. 

v 1.2.1 (Dec 16 2025 14:30:00)
- добавлена команда REQ_CMD_GET_CMD_INFO (03 04) для получения информации о последней команде
- добавлена команда REQ_CMD_GET_CMD_INFO (03 04) для получения информации о последней команде. В аудит 
команды записываются только команды, но не ответы по телеметрии.
- откорректирована передача информации на сервер:
пересечение было возможно: передача могла «съедать» события приёма, потому что PR_RX_Compl_SemHandle 
использовался и для CommandReceiver, и для ожиданий перед TX.
Сейчас пересечение исключено:
приём команд и ожидание “RX кадр завершён” разведены на разные семафоры (PR_RX_Compl_SemHandle и UART4_RX_Event_SemHandle);
перед любым TX по UART4 выполняется ожидание завершения активного приёма (если реально идут байты), а приём останавливается (AbortReceive) только непосредственно перед передачей под UART4_Mutex;
ответы на команды/повтор телеметрии идут через high-priority отправку и не блокируются телеметрией.
*/