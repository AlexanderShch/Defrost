################################################################################
# Устойчивое восстановление сборки после неполной регенерации Debug/makefile.
# Подключается из makefile.defs ТОЛЬКО если нет ./Application/User/main.o в OBJS.
# CubeIDE этот файл не перезаписывает.
################################################################################

# Nested subdir.mk, которые CubeIDE часто выкидывает из корневого makefile
-include Middlewares/FreeRTOS/subdir.mk
-include Drivers/STM32F4xx_HAL_Driver/subdir.mk
-include Drivers/CMSIS/subdir.mk
-include Drivers/BSP/Components/subdir.mk
-include Application/User/gui/subdir.mk
-include Application/User/generated/subdir.mk
-include Application/User/TouchGFX/target/generated/subdir.mk
-include Application/User/TouchGFX/target/subdir.mk
-include Application/User/TouchGFX/App/subdir.mk

# Linked Core/Src (в .project как Application/User/*), которые CubeIDE
# иногда не добавляет в Application/User/subdir.mk
DEFROST_CORE_CFLAGS := -mcpu=cortex-m4 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DUSE_BPP=16 -DDEBUG -DSTM32F429xx -c \
	-I../../Core/Inc -I../../Drivers/CMSIS/Include -I../../Drivers/BSP -I../../TouchGFX/target \
	-I../../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../../TouchGFX/App \
	-I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../TouchGFX/target/generated \
	-I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy \
	-I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../../Drivers/STM32F4xx_HAL_Driver/Inc \
	-I../../Middlewares/ST/touchgfx/framework/include -I../../TouchGFX/generated/fonts/include \
	-I../../TouchGFX/generated/gui_generated/include -I../../TouchGFX/generated/images/include \
	-I../../TouchGFX/generated/texts/include -I../../TouchGFX/gui/include -I../../TouchGFX/generated/videos/include \
	-I"D:/ST/Defrost/STM32CubeIDE/ProjectCode" -I../../TouchGFX/gui/include/gui/settings1_screen \
	-Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity \
	--specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb

C_SRCS += \
D:/ST/Defrost/Core/Src/freertos.c \
D:/ST/Defrost/Core/Src/main.c \
D:/ST/Defrost/Core/Src/stm32f4xx_hal_msp.c \
D:/ST/Defrost/Core/Src/stm32f4xx_hal_timebase_tim.c \
D:/ST/Defrost/Core/Src/stm32f4xx_it.c

C_DEPS += \
./Application/User/freertos.d \
./Application/User/main.d \
./Application/User/stm32f4xx_hal_msp.d \
./Application/User/stm32f4xx_hal_timebase_tim.d \
./Application/User/stm32f4xx_it.d

OBJS += \
./Application/User/freertos.o \
./Application/User/main.o \
./Application/User/stm32f4xx_hal_msp.o \
./Application/User/stm32f4xx_hal_timebase_tim.o \
./Application/User/stm32f4xx_it.o

Application/User/freertos.o: D:/ST/Defrost/Core/Src/freertos.c
	arm-none-eabi-gcc "$<" $(DEFROST_CORE_CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@"
Application/User/main.o: D:/ST/Defrost/Core/Src/main.c
	arm-none-eabi-gcc "$<" $(DEFROST_CORE_CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@"
Application/User/stm32f4xx_hal_msp.o: D:/ST/Defrost/Core/Src/stm32f4xx_hal_msp.c
	arm-none-eabi-gcc "$<" $(DEFROST_CORE_CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@"
Application/User/stm32f4xx_hal_timebase_tim.o: D:/ST/Defrost/Core/Src/stm32f4xx_hal_timebase_tim.c
	arm-none-eabi-gcc "$<" $(DEFROST_CORE_CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@"
Application/User/stm32f4xx_it.o: D:/ST/Defrost/Core/Src/stm32f4xx_it.c
	arm-none-eabi-gcc "$<" $(DEFROST_CORE_CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@"
