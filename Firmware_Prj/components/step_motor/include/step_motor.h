/*
 * SPDX-FileCopyrightText: Copyright 2025 JeongYeham
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STEP_MOTOR_H
#define STEP_MOTOR_H

#include <stdbool.h>

typedef struct stepper_cmd
{
    int steps;
    bool dir_cw;
    int speed_us;
} stepper_cmd_t;


void stepper_driver_init(void);
void stepper_rotate_angle(float degree, bool cw, int rpm);
void stepper_set_motion(int steps, bool dir, int speed_us);

#endif
