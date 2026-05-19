#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "cmsis_os.h"
#include <stdint.h>

extern osMessageQueueId_t temp_queue;

void SensorTask(void *argument);
void LoggerTask(void *argument);

#endif /* APP_TASKS_H */