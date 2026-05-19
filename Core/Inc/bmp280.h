#ifndef BMP280_H
#define BMP280_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define BMP280_I2C_ADDR     (0x77U << 1)
#define BMP280_REG_ID       0xD0
#define BMP280_REG_RESET    0xE0
#define BMP280_REG_CTRL     0xF4
#define BMP280_REG_CALIB    0x88
#define BMP280_REG_TEMP_MSB 0xFA
/* ctrl_meas register fields */
#define BMP280_OSRS_T_x1    (0x01U << 5)   /* temp oversampling x1  */
#define BMP280_OSRS_P_x1    (0x01U << 2)   /* press oversampling x1 */
#define BMP280_MODE_NORMAL  (0x03U << 0)   /* normal mode           */

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
} BMP280_CalibData;

HAL_StatusTypeDef BMP280_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef BMP280_ReadCalibration(I2C_HandleTypeDef *hi2c, BMP280_CalibData *calib);
HAL_StatusTypeDef BMP280_ReadTemperature(I2C_HandleTypeDef *hi2c, const BMP280_CalibData *calib, int32_t *temp_x100);
void BMP280_ParseCalibration(const uint8_t *buf, BMP280_CalibData *calib);
int32_t BMP280_CompensateTemp(const BMP280_CalibData *calib, int32_t adc_T);

#endif /* BMP280_H */