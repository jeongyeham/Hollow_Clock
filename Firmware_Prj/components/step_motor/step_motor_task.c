/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <sys/cdefs.h>
#include "step_motor_task.h"
static const char *TAG = "hc_step_motor";

esp_err_t esp_rotate_step_motor(dedic_gpio_bundle_handle_t dedic_gpio_bundle_handle, int step_phase);

static portMUX_TYPE spinlock;
static const uint8_t code_octa_phase[8] = {0x08, 0x0c, 0x04, 0x06, 0x02, 0x03, 0x01, 0x09};
//static const uint8_t code_double_qaudra_phase[4] = {0x0c, 0x06, 0x03, 0x09};
//static const uint8_t code_single_quadra_phase[4] = {0x08, 0x04, 0x02, 0x01};


static bool IRAM_ATTR gptimer_on_alarm_cb(__attribute__((unused)) gptimer_handle_t timer, __attribute__((unused)) const gptimer_alarm_event_data_t *edata, void *user_data)
{
    dedic_gpio_bundle_handle_t *cb_dedic_gpio_bundle_handle = (dedic_gpio_bundle_handle_t *)user_data;
    portENTER_CRITICAL_ISR(&spinlock);
    dedic_gpio_bundle_write(*cb_dedic_gpio_bundle_handle, 1UL << 1, 1UL << 1);
    portEXIT_CRITICAL_ISR(&spinlock);

    // return whether we need to yield at the end of ISR
    return 0;
}

_Noreturn void step_motor_task(void *param)
{
    dedic_gpio_bundle_handle_t dedic_gpio_bundle_handle;

    portMUX_INITIALIZE(&spinlock);

    // Configure GPIO
    const int bundle_gpios[] = {4, 5, 6, 7};
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
    };
    for (int i = 0; i < sizeof(bundle_gpios) / sizeof(bundle_gpios[0]); i++)
    {
        io_conf.pin_bit_mask = 1ULL << bundle_gpios[i];
        gpio_config(&io_conf);
    }
    // Create bundle，only for output
    dedic_gpio_bundle_config_t bundle_config = {
        .gpio_array = bundle_gpios,
        .array_size = sizeof(bundle_gpios) / sizeof(bundle_gpios[0]),
        .flags = {
            .out_en = 1,
        },
    };
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&bundle_config, &dedic_gpio_bundle_handle));

    ESP_LOGI(TAG, "Create timer handle");
    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = gptimer_on_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, &dedic_gpio_bundle_handle));

    ESP_LOGI(TAG, "Enable timer");
    // ESP_ERROR_CHECK(gptimer_enable(gptimer));

    ESP_LOGI(TAG, "Start timer, auto-reload at alarm event");
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 1000000, // period = 1s
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    // ESP_ERROR_CHECK(gptimer_start(gptimer));
    while (1)
    {
        esp_rotate_step_motor(dedic_gpio_bundle_handle, 360 * 9);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    ESP_LOGI(TAG, "Stop timer");
    ESP_ERROR_CHECK(gptimer_stop(gptimer));
}

esp_err_t esp_rotate_step_motor(dedic_gpio_bundle_handle_t dedic_gpio_bundle_handle, int step_phase)
{
    int step;
    step = step_phase * 4096 / 360;
    for (size_t j = 0; j < step; j += 8)
    {
        for (size_t motor_phase = 0; motor_phase < 8; motor_phase++)
        {
            portENTER_CRITICAL(&spinlock);
            dedic_gpio_bundle_write(dedic_gpio_bundle_handle, (uint32_t)0b1111, (uint32_t)code_octa_phase[motor_phase]);
            esp_rom_delay_us(900);
            portEXIT_CRITICAL(&spinlock);
        }
    }
    portENTER_CRITICAL(&spinlock);
    dedic_gpio_bundle_write(dedic_gpio_bundle_handle, (uint32_t)0b1111, (uint32_t)0x00);
    portEXIT_CRITICAL(&spinlock);
    return ESP_OK;
}