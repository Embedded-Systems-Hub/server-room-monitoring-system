#ifndef HAL_I2C_STUB_H
#define HAL_I2C_STUB_H

#include "stm32f4xx_hal.h"

void HAL_I2C_Stub_SetReadData(const uint8_t *data, uint16_t len);
void HAL_I2C_Stub_SetReturnValue(HAL_StatusTypeDef ret);

#endif /* HAL_I2C_STUB_H */