//
// Created by jeong on 2025/5/27.
//
#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H


void step_motor_task(void *pvParameters);
void initialise_wifi_task(void *pvParameters);
void smartconfig_task(void *pvParameters);
void motor_control_task(void *pvParameters);

#endif //FREERTOS_TASK_H