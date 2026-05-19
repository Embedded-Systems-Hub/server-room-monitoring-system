#include "app_tasks.h"
#include "bmp280.h"
#include "i2c.h"
#include "usart.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern BMP280_CalibData bmp280_calib;
#define TEMP_ALERT_THRESHOLD_x100  3000

void SensorTask(void *argument) {
    int32_t temp_x100 = 0;

    for (;;) {
        BMP280_ReadTemperature(&hi2c1, &bmp280_calib, &temp_x100);
        osMessageQueuePut(temp_queue, &temp_x100, 0, 0);
        osDelay(1000);
    }
}

void LoggerTask(void *argument) {
    int32_t temp_x100 = 0;
    char msg[64];

    for (;;) {
        osMessageQueueGet(temp_queue, &temp_x100, NULL, osWaitForever);

        snprintf(msg, sizeof(msg), "[TEMP] %ld.%02ld C\r\n",
                (long)(temp_x100 / 100), (long)(temp_x100 % 100));
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

        if (temp_x100 >= TEMP_ALERT_THRESHOLD_x100) {
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
        }
    }
}