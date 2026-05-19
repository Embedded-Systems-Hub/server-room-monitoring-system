#include "unity.h"
#include "bmp280.h"

void setUp(void) {}
void tearDown(void) {}

void test_ParseCalibration_correctly_parses_little_endian(void) {
    uint8_t buf[6] = { 0x70, 0x6B, 0x43, 0x67, 0xD0, 0x01 };
    BMP280_CalibData calib;

    BMP280_ParseCalibration(buf, &calib);

    TEST_ASSERT_EQUAL_UINT16(0x6B70, calib.dig_T1);
    TEST_ASSERT_EQUAL_INT16 (0x6743, calib.dig_T2);
    TEST_ASSERT_EQUAL_INT16 (0x01D0, calib.dig_T3);
}

void test_CompensateTemp_returns_expected_value(void) {
    BMP280_CalibData calib = {
        .dig_T1 = 27504,
        .dig_T2 = 26435,
        .dig_T3 = -1000
    };
    int32_t adc_T = 519888;

    int32_t result = BMP280_CompensateTemp(&calib, adc_T);

    TEST_ASSERT_INT32_WITHIN(50, 2508, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ParseCalibration_correctly_parses_little_endian);
    RUN_TEST(test_CompensateTemp_returns_expected_value);
    return UNITY_END();
}