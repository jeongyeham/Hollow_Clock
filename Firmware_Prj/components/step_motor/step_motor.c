/*
 * SPDX-FileCopyrightText: Copyright 2025 JeongYeham
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "step_motor.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <stdint.h>

#define STEPS_PER_REV 4096
#define MIN_SPEED_US 100  // 对应最大10kHz频率
#define MOTOR_TAG "STEP_MOTOR"

static const uint8_t code_octa_phase[8] = {0x08, 0x0C, 0x04, 0x06, 0x02, 0x03, 0x01, 0x09};

/* 定时器回调（ISR）*/
static bool IRAM_ATTR gptimer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata,
                                          void* user_data)
{
    motor_control_t* motor_control_isr = (motor_control_t*)user_data;

    if unlikely(motor_control_isr->motion.executed_steps >= motor_control_isr->motion.total_steps)
    {
        gptimer_stop(timer);
        taskENTER_CRITICAL_ISR(motor_control_isr->motor_spinlock);
        dedic_gpio_bundle_write(motor_control_isr->motor_dedic_gpio_bundle, 0x1111, 0x0000);
        taskEXIT_CRITICAL_ISR(motor_control_isr->motor_spinlock);
        return false;
    }

    uint8_t phase = code_octa_phase[motor_control_isr->motion.step_index & 0x07];
    taskENTER_CRITICAL_ISR(motor_control_isr->motor_spinlock);
    dedic_gpio_bundle_write(motor_control_isr->motor_dedic_gpio_bundle, 0x1111, phase);
    taskEXIT_CRITICAL_ISR(motor_control_isr->motor_spinlock);

    // 更新步进索引和位置
    motor_control_isr->motion.step_index += motor_control_isr->motion.direction_cw ? 1 : -1;
    motor_control_isr->motion.step_index &= 0x07;
    motor_control_isr->motion.absolute_position += motor_control_isr->motion.direction_cw ? 1 : -1;
    motor_control_isr->motion.executed_steps++;

    return true;
}

/* 设置运动参数 */
void stepper_set_time(motor_control_t* motor_control, int steps, bool dir, int speed_us)
{
    if (speed_us < MIN_SPEED_US) speed_us = MIN_SPEED_US;

    motor_control->motion.direction_cw = dir;
    motor_control->motion.total_steps = steps;
    motor_control->motion.executed_steps = 0;
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = speed_us,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(motor_control->motor_gptimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_set_raw_count(motor_control->motor_gptimer, 0));
    ESP_ERROR_CHECK(gptimer_start(motor_control->motor_gptimer));
    ESP_LOGD(MOTOR_TAG, "Motion: %d steps %s at %dus/step",
             steps, dir ? "CW" : "CCW", speed_us);
}

/* 获取绝对位置 */
int stepper_get_position(const motor_control_t* motor_control)
{
    return motor_control->motion.absolute_position;
}

/* 位置归零或设定 */
void stepper_set_position(motor_control_t* motor_control, int position)
{
    motor_control->motion.absolute_position = position;
    ESP_LOGD(MOTOR_TAG, "Position set to %d", position);
}

/* 初始化驱动 */
motor_control_t* stepper_driver_init(void)
{
    const int bundle_gpios[] = {35, 36, 37, 38};

    ////////////////////////////////////////////////////////////////// GPIO配置
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    for (int i = 0; i < sizeof(bundle_gpios) / sizeof(bundle_gpios[0]); i++)
    {
        io_conf.pin_bit_mask |= 1ULL << bundle_gpios[i];
    }
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //////////////////////////////////////////////////////////////////////////////封装结构体
    motor_control_t* motor_control = pvPortMalloc(sizeof(motor_control_t));
    if (!motor_control)
    {
        ESP_LOGE(MOTOR_TAG, "Failed to allocate motor_control");
        return NULL;
    }
    memset(motor_control, 0, sizeof(motor_control_t));

    ///////////////////////////////////////////////////////////////// 自旋锁
    motor_control->motor_spinlock = pvPortMalloc(sizeof(portMUX_TYPE));
    if (!motor_control->motor_spinlock)
    {
        ESP_LOGE(MOTOR_TAG, "Failed to allocate spinlock");
        return NULL;
    }
    portMUX_INITIALIZE(motor_control->motor_spinlock);


    ///////////////////////////////////////////////////////////////// 专用GPIO束配置
    dedic_gpio_bundle_config_t bundle_config = {
        .gpio_array = bundle_gpios,
        .array_size = sizeof(bundle_gpios) / sizeof(bundle_gpios[0]),
        .flags = {.out_en = 1},
    };
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&bundle_config, &motor_control->motor_dedic_gpio_bundle));

    ///////////////////////////////////////////////////////////////// GPTimer配置（1MHz分辨率）
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &motor_control->motor_gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = gptimer_on_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(motor_control->motor_gptimer, &cbs, &motor_control));
    ESP_ERROR_CHECK(gptimer_enable(motor_control->motor_gptimer));

    // 初始状态
    taskENTER_CRITICAL(motor_control->motor_spinlock);
    dedic_gpio_bundle_write(motor_control->motor_dedic_gpio_bundle, 0x1111, 0x0000);
    taskEXIT_CRITICAL(motor_control->motor_spinlock);

    ESP_LOGI(MOTOR_TAG, "Driver initialized @ GPIO%d-%d",
             bundle_gpios[0], bundle_gpios[3]);

    return motor_control;
}

/* 反初始化释放资源 */
void stepper_driver_deinit(motor_control_t* motor_control)
{
    taskENTER_CRITICAL_ISR(motor_control->motor_spinlock);
    dedic_gpio_bundle_write(motor_control->motor_dedic_gpio_bundle, 0x1111, 0x0000);
    taskEXIT_CRITICAL_ISR(motor_control->motor_spinlock);

    dedic_gpio_del_bundle(motor_control->motor_dedic_gpio_bundle);
    motor_control->motor_dedic_gpio_bundle = NULL;

    gptimer_disable(motor_control->motor_gptimer);
    gptimer_del_timer(motor_control->motor_gptimer);

    free(motor_control);
    ESP_LOGI(MOTOR_TAG, "Driver deinitialized");
}
