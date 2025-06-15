/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "FreeRTOS_task.h"
#include "main.h"

char *TAG = "app_main";

_Noreturn void app_main(void)
{
    TaskHandle_t motor_task_handle;
    TaskHandle_t motor_control_task_handle;

    user_data_t cb_user_data = {
        .all_event = xEventGroupCreate(),
    };
    
    xEventGroupClearBits(cb_user_data.all_event, 0xff);

    xTaskCreatePinnedToCore(step_motor_task, "step_motor", 4096, &cb_user_data, 0, &motor_task_handle,0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    xTaskCreatePinnedToCore(motor_control_task, "motor_control", 4096, &cb_user_data, 0, &motor_control_task_handle, tskNO_AFFINITY);
    while (1)
    {
        ESP_LOGI(TAG, "hello");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}