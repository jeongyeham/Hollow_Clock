/*
 * SPDX-FileCopyrightText: Copyright 2025 JeongYeham
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/event_groups.h"
#include "step_motor.h"

// 添加时间结构体定义
typedef struct {
    int hour;
    int minute;
    int second;
} clock_time_t;

// 添加时钟状态枚举定义
typedef enum {
    CLOCK_STATE_IDLE,        // 空闲状态
    CLOCK_STATE_MOVING,      // 移动状态
    CLOCK_STATE_ADJUSTING,   // 调整状态
    CLOCK_STATE_ERROR        // 错误状态
} clock_state_t;

// 前向声明
typedef struct clock_control_handle clock_control_handle_t;

typedef struct
{
    EventGroupHandle_t all_event;
    motor_control_t* motor_control;
    clock_state_t clock_state;         // 添加时钟状态字段
    bool watchdog_enabled;             // 添加看门狗使能字段
    clock_time_t current_time;         // 添加当前时间字段
    clock_time_t target_time;          // 添加目标时间字段
} user_data_t;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

#define CLOCK_MOVE_COMPLETE_BIT   BIT0
#define CLOCK_ADJUST_TIME_BIT     BIT1

#ifdef __cplusplus
}
#endif

#endif