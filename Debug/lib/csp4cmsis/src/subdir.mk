################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../lib/csp4cmsis/src/alt_channel_sync.cpp \
../lib/csp4cmsis/src/alternative.cpp \
../lib/csp4cmsis/src/barrier.cpp \
../lib/csp4cmsis/src/buffered_channel.cpp \
../lib/csp4cmsis/src/channel_sync.cpp \
../lib/csp4cmsis/src/csp_wrapper.cpp \
../lib/csp4cmsis/src/glue.cpp \
../lib/csp4cmsis/src/kernel.cpp \
../lib/csp4cmsis/src/sync_channel.cpp 

OBJS += \
./lib/csp4cmsis/src/alt_channel_sync.o \
./lib/csp4cmsis/src/alternative.o \
./lib/csp4cmsis/src/barrier.o \
./lib/csp4cmsis/src/buffered_channel.o \
./lib/csp4cmsis/src/channel_sync.o \
./lib/csp4cmsis/src/csp_wrapper.o \
./lib/csp4cmsis/src/glue.o \
./lib/csp4cmsis/src/kernel.o \
./lib/csp4cmsis/src/sync_channel.o 

CPP_DEPS += \
./lib/csp4cmsis/src/alt_channel_sync.d \
./lib/csp4cmsis/src/alternative.d \
./lib/csp4cmsis/src/barrier.d \
./lib/csp4cmsis/src/buffered_channel.d \
./lib/csp4cmsis/src/channel_sync.d \
./lib/csp4cmsis/src/csp_wrapper.d \
./lib/csp4cmsis/src/glue.d \
./lib/csp4cmsis/src/kernel.d \
./lib/csp4cmsis/src/sync_channel.d 


# Each subdirectory must supply rules for building sources it contributes
lib/csp4cmsis/src/%.o lib/csp4cmsis/src/%.su lib/csp4cmsis/src/%.cyclo: ../lib/csp4cmsis/src/%.cpp lib/csp4cmsis/src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"/media/of6/c615665c-b372-4e0b-8c40-ca60b0369c46/The_Way_of_Static_Process_Networks/GithubCode/CSP4CMSIS_NUCLEO-G474RE_Shake_Detection_L3G4200D/lib/csp4cmsis/inc" -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-lib-2f-csp4cmsis-2f-src

clean-lib-2f-csp4cmsis-2f-src:
	-$(RM) ./lib/csp4cmsis/src/alt_channel_sync.cyclo ./lib/csp4cmsis/src/alt_channel_sync.d ./lib/csp4cmsis/src/alt_channel_sync.o ./lib/csp4cmsis/src/alt_channel_sync.su ./lib/csp4cmsis/src/alternative.cyclo ./lib/csp4cmsis/src/alternative.d ./lib/csp4cmsis/src/alternative.o ./lib/csp4cmsis/src/alternative.su ./lib/csp4cmsis/src/barrier.cyclo ./lib/csp4cmsis/src/barrier.d ./lib/csp4cmsis/src/barrier.o ./lib/csp4cmsis/src/barrier.su ./lib/csp4cmsis/src/buffered_channel.cyclo ./lib/csp4cmsis/src/buffered_channel.d ./lib/csp4cmsis/src/buffered_channel.o ./lib/csp4cmsis/src/buffered_channel.su ./lib/csp4cmsis/src/channel_sync.cyclo ./lib/csp4cmsis/src/channel_sync.d ./lib/csp4cmsis/src/channel_sync.o ./lib/csp4cmsis/src/channel_sync.su ./lib/csp4cmsis/src/csp_wrapper.cyclo ./lib/csp4cmsis/src/csp_wrapper.d ./lib/csp4cmsis/src/csp_wrapper.o ./lib/csp4cmsis/src/csp_wrapper.su ./lib/csp4cmsis/src/glue.cyclo ./lib/csp4cmsis/src/glue.d ./lib/csp4cmsis/src/glue.o ./lib/csp4cmsis/src/glue.su ./lib/csp4cmsis/src/kernel.cyclo ./lib/csp4cmsis/src/kernel.d ./lib/csp4cmsis/src/kernel.o ./lib/csp4cmsis/src/kernel.su ./lib/csp4cmsis/src/sync_channel.cyclo ./lib/csp4cmsis/src/sync_channel.d ./lib/csp4cmsis/src/sync_channel.o ./lib/csp4cmsis/src/sync_channel.su

.PHONY: clean-lib-2f-csp4cmsis-2f-src

