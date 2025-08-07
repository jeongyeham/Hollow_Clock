//
// Created by jeong on 2025/5/27.
//
#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H

#include "main.h"

// 添加时钟控制句柄结构体定义
typedef struct clock_control_handle {
    user_data_t* user_data;
    int target_hour;
    int target_minute;
    float target_angle;
    bool initialized;  // 添加初始化标志字段
} clock_control_handle_t;

void step_motor_task(void *pvParameters);
void initialise_wifi_task(void *pvParameters);
void smartconfig_task(void *pvParameters);
void motor_control_task(void *pvParameters);
void clock_control_task(void *pvParameters); // 新增的时钟控制任务

// 时钟控制句柄相关函数
void clock_control_handler(clock_control_handle_t* handle);
void set_clock_target_time(clock_control_handle_t* handle, int hour, int minute);
clock_state_t get_clock_state(clock_control_handle_t* handle);

#endif //FREERTOS_TASK_H