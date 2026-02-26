/*
 * DefrostControl.h
 * C API алгоритма управления разморозкой (включение/выключение, инициализация, периодическое обновление).
 */

#ifndef DEFROSTCONTROL_H
#define DEFROSTCONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void DefrostControl_Init(void);
void DefrostControl_SetEnabled(uint8_t enabled);
void DefrostControl_Update1s(void);
uint32_t DefrostControl_GetRuntimeSeconds(void);
uint8_t DefrostControl_IsEnabled(void);

#ifdef __cplusplus
}
#endif

#endif /* DEFROSTCONTROL_H */
