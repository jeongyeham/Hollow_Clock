/*
 * SPDX-FileCopyrightText: Copyright 2025 JeongYeham
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "step_motor.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/dedic_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <stdint.h>

#define STEPS_PER_REV 4096
#define MOTOR_TAG "STEP_MOTOR"

static const uint8_t code_octa_phase[8] = {0x08, 0x0C, 0x04, 0x06, 0x02, 0x03, 0x01, 0x09};

static volatile int step_index = 0;
static volatile int remaining_steps = 0;
static volatile bool direction_cw = true;

static gptimer_handle_t gptimer = NULL;
static dedic_gpio_bundle_handle_t dedic_gpio_bundle_handle = NULL;



static bool IRAM_ATTR gptimer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata,
                                          void* user_data)
{
    dedic_gpio_bundle_handle_t dedic_gpio_bundle_handle = *(dedic_gpio_bundle_handle_t*)user_data;
    if (remaining_steps <= 0)
    {
        gptimer_stop(timer);
        dedic_gpio_bundle_write(dedic_gpio_bundle_handle, 0x1111, 0x00);
        return false;
    }

    uint8_t phase = code_octa_phase[step_index & 0x07];
    dedic_gpio_bundle_write(dedic_gpio_bundle_handle, 0x1111, phase);

    step_index += direction_cw ? 1 : -1;
    if (step_index < 0) step_index = 7;
    else step_index &= 0x07;

    remaining_steps--;
    return true;
}

void stepper_set_motion(int steps, bool dir, int speed_us)
{
    direction_cw = dir;
    step_index = 0;
    remaining_steps = steps;

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = speed_us,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(gptimer, &alarm_config);
    gptimer_start(gptimer);
}

void stepper_driver_init(void)
{
    const int bundle_gpios[] = {35, 36, 37, 38};
    gpio_config_t io_conf = {.mode = GPIO_MODE_OUTPUT};
    for (int i = 0; i < sizeof(bundle_gpios) / sizeof(bundle_gpios[0]); i++)
    {
        io_conf.pin_bit_mask = 1ULL << bundle_gpios[i];
        gpio_config(&io_conf);
    }

    dedic_gpio_bundle_config_t bundle_config = {
        .gpio_array = bundle_gpios,
        .array_size = sizeof(bundle_gpios) / sizeof(bundle_gpios[0]),
        .flags = {.out_en = 1},
    };
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&bundle_config, &dedic_gpio_bundle_handle));

    ESP_LOGI(MOTOR_TAG, "Create timer handle");
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = gptimer_on_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
}


