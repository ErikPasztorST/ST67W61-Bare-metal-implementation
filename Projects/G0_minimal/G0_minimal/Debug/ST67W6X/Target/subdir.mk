################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ST67W6X/Target/spi_port.c 

OBJS += \
./ST67W6X/Target/spi_port.o 

C_DEPS += \
./ST67W6X/Target/spi_port.d 


# Each subdirectory must supply rules for building sources it contributes
ST67W6X/Target/%.o ST67W6X/Target/%.su ST67W6X/Target/%.cyclo: ../ST67W6X/Target/%.c ST67W6X/Target/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../ST67W6X/Target -I../ST67W6X/Drivers -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -Oz -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-ST67W6X-2f-Target

clean-ST67W6X-2f-Target:
	-$(RM) ./ST67W6X/Target/spi_port.cyclo ./ST67W6X/Target/spi_port.d ./ST67W6X/Target/spi_port.o ./ST67W6X/Target/spi_port.su

.PHONY: clean-ST67W6X-2f-Target

