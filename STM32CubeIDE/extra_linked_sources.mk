# Linked Core/Src, которые генератор 14.3 не кладёт в Application/User/subdir.mk.

CORE_SRC := D:/ST/Defrost/Core/Src

C_SRCS += \
$(CORE_SRC)/main.c \
$(CORE_SRC)/freertos.c \
$(CORE_SRC)/stm32f4xx_hal_msp.c \
$(CORE_SRC)/stm32f4xx_hal_timebase_tim.c \
$(CORE_SRC)/stm32f4xx_it.c

C_DEPS += \
./Application/User/main.d \
./Application/User/freertos.d \
./Application/User/stm32f4xx_hal_msp.d \
./Application/User/stm32f4xx_hal_timebase_tim.d \
./Application/User/stm32f4xx_it.d

OBJS += \
./Application/User/main.o \
./Application/User/freertos.o \
./Application/User/stm32f4xx_hal_msp.o \
./Application/User/stm32f4xx_hal_timebase_tim.o \
./Application/User/stm32f4xx_it.o

APP_USER_CORE_CFLAGS := -mcpu=cortex-m4 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DUSE_BPP=16 -DDEBUG -DSTM32F429xx -c \
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
-MMD -MP --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb

./Application/User/main.o ./Application/User/freertos.o ./Application/User/stm32f4xx_hal_msp.o ./Application/User/stm32f4xx_hal_timebase_tim.o ./Application/User/stm32f4xx_it.o: ./Application/User/%.o: $(CORE_SRC)/%.c
	arm-none-eabi-gcc "$<" $(APP_USER_CORE_CFLAGS) -MF"$(@:%.o=%.d)" -MT"$@" -o "$@"
