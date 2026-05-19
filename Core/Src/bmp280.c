#include "bmp280.h"

HAL_StatusTypeDef BMP280_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t ctrl = BMP280_OSRS_T_x1 | BMP280_OSRS_P_x1 | BMP280_MODE_NORMAL;
    return HAL_I2C_Mem_Write(hi2c, BMP280_I2C_ADDR,
                             BMP280_REG_CTRL, I2C_MEMADD_SIZE_8BIT,
                             &ctrl, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef BMP280_ReadCalibration(I2C_HandleTypeDef *hi2c, BMP280_CalibData *calib) {
    uint8_t buf[6];
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDR,
                                              BMP280_REG_CALIB, I2C_MEMADD_SIZE_8BIT,
                                              buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }
    BMP280_ParseCalibration(buf, calib);
    return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadTemperature(I2C_HandleTypeDef *hi2c, const BMP280_CalibData *calib, int32_t *temp_x100) {
    uint8_t buf[3];
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDR,
                                              BMP280_REG_TEMP_MSB, I2C_MEMADD_SIZE_8BIT,
                                              buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    int32_t adc_T = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    *temp_x100 = BMP280_CompensateTemp(calib, adc_T);
    return HAL_OK;
}

void BMP280_ParseCalibration(const uint8_t *buf, BMP280_CalibData *calib) {
    calib->dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    calib->dig_T2 = (int16_t) (buf[3] << 8 | buf[2]);
    calib->dig_T3 = (int16_t) (buf[5] << 8 | buf[4]);
}

int32_t BMP280_CompensateTemp(const BMP280_CalibData *calib, int32_t adc_T) {
    int32_t var1 = ((adc_T >> 3) - ((int32_t)calib->dig_T1 << 1));
    var1 = (var1 * (int32_t)calib->dig_T2) >> 11;

    int32_t var2 = ((adc_T >> 4) - (int32_t)calib->dig_T1);
    var2 = (((var2 * var2) >> 12) * (int32_t)calib->dig_T3) >> 14;

    return ((var1 + var2) * 5 + 128) >> 8;
}
