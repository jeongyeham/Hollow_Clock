/*
 * SPDX-FileCopyrightText: Copyright 2025 JeongYeham
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STEP_MOTOR_H
#define STEP_MOTOR_H

#include <stdbool.h>
#include "time.h"

#include "driver/dedic_gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct stepper_cmd
{
    int steps;
    bool dir_cw;
    int speed_us;
} stepper_cmd_t;

typedef struct motor_motion
{
    int total_steps;
    int executed_steps;
    int step_index;
    bool direction_cw;
    int absolute_position; // 绝对位置记录
} motor_motion_t;

typedef struct motor_control
{
    motor_motion_t motion;
    dedic_gpio_bundle_handle_t motor_dedic_gpio_bundle;
    gptimer_handle_t motor_gptimer;
    void* motor_spinlock;
    QueueHandle_t motor_cmd_queue;
     last_step_time;
}motor_control_t;


motor_control_t* stepper_driver_init(void);
void stepper_rotate_angle(float degree, bool cw, float rpm);
void stepper_set_motion(motor_control_t* motor_control, int steps, bool dir, int speed_us);

#endif
