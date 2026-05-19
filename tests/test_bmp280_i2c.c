#include "unity.h"
#include "bmp280.h"
#include "hal_i2c_stub.h"

static I2C_HandleTypeDef hi2c_stub;

void setUp(void) {
    HAL_I2C_Stub_SetReturnValue(HAL_OK);
}

void tearDown(void) {}

void test_ReadCalibration_parses_i2c_data_correctly(void) {
    uint8_t raw[6] = { 0x70, 0x6B, 0x43, 0x67, 0xD0, 0x01 };
    HAL_I2C_Stub_SetReadData(raw, sizeof(raw));

    BMP280_CalibData calib;
    HAL_StatusTypeDef ret = BMP280_ReadCalibration(&hi2c_stub, &calib);

    TEST_ASSERT_EQUAL(HAL_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(0x6B70, calib.dig_T1);
    TEST_ASSERT_EQUAL_INT16 (0x6743, calib.dig_T2);
    TEST_ASSERT_EQUAL_INT16 (0x01D0, calib.dig_T3);
}

void test_ReadCalibration_returns_error_on_i2c_failure(void) {
    HAL_I2C_Stub_SetReturnValue(HAL_ERROR);

    BMP280_CalibData calib;
    HAL_StatusTypeDef ret = BMP280_ReadCalibration(&hi2c_stub, &calib);

    TEST_ASSERT_EQUAL(HAL_ERROR, ret);
}

void test_ReadTemperature_returns_error_on_i2c_failure(void) {
    HAL_I2C_Stub_SetReturnValue(HAL_ERROR);

    BMP280_CalibData calib = { .dig_T1 = 27504, .dig_T2 = 26435, .dig_T3 = -1000 };
    int32_t temp_x100;
    HAL_StatusTypeDef ret = BMP280_ReadTemperature(&hi2c_stub, &calib, &temp_x100);

    TEST_ASSERT_EQUAL(HAL_ERROR, ret);
}

void test_ReadTemperature_returns_compensated_value(void) {
    uint8_t raw[3] = { 0x7F, 0x80, 0x00 };
    HAL_I2C_Stub_SetReadData(raw, sizeof(raw));

    BMP280_CalibData calib = { .dig_T1 = 27504, .dig_T2 = 26435, .dig_T3 = -1000 };
    int32_t temp_x100;
    HAL_StatusTypeDef ret = BMP280_ReadTemperature(&hi2c_stub, &calib, &temp_x100);

    TEST_ASSERT_EQUAL(HAL_OK, ret);
    TEST_ASSERT_INT32_WITHIN(200, 2500, temp_x100);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ReadCalibration_parses_i2c_data_correctly);
    RUN_TEST(test_ReadCalibration_returns_error_on_i2c_failure);
    RUN_TEST(test_ReadTemperature_returns_error_on_i2c_failure);
    RUN_TEST(test_ReadTemperature_returns_compensated_value);
    return UNITY_END();
}