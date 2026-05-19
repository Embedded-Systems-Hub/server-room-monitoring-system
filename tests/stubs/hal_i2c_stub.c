#include "stm32f4xx_hal.h"
#include <string.h>

static uint8_t stub_buf[8];
static HAL_StatusTypeDef stub_ret = HAL_OK;

void HAL_I2C_Stub_SetReadData(const uint8_t *data, uint16_t len) {
    memcpy(stub_buf, data, len);
}

void HAL_I2C_Stub_SetReturnValue(HAL_StatusTypeDef ret) {
    stub_ret = ret;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize,
                                    uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)hi2c; (void)DevAddress; (void)MemAddress;
    (void)MemAddSize; (void)Timeout;
    memcpy(pData, stub_buf, Size);
    return stub_ret;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                     uint16_t MemAddress, uint16_t MemAddSize,
                                     uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)hi2c; (void)DevAddress; (void)MemAddress;
    (void)MemAddSize; (void)pData; (void)Size; (void)Timeout;
    return stub_ret;
}