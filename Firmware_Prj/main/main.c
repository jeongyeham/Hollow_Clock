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
    user_data_t cb_user_data = {
        .all_event = xEventGroupCreate(),
    };
    
    xEventGroupClearBits(cb_user_data.all_event, 0xff);

    xTaskCreate(step_motor_task, "step_motor", 4096, &cb_user_data, 0, &motor_task_handle);
    /*while (1)
    {
        EventBits_t uxBits_wifi = xEventGroupWaitBits(cb_user_data.all_event, ESPTOUCH_DONE_BIT, pdFALSE, pdTRUE, (TickType_t)0);

        if (unlikely(uxBits_wifi & (ESPTOUCH_DONE_BIT)))
        {
            ESP_LOGI("MAIN", "Network found, prepare to connect SNTP");
            setenv("TZ", "EST-8", 1);
            tzset();
            sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
            esp_sntp_setservername(0, "ntp1.aliyun.com");
            esp_sntp_setservername(1, "ntp2.aliyun.com");
            esp_sntp_setservername(2, "ntp3.aliyun.com");
            esp_sntp_init();
            xEventGroupSetBits(cb_user_data.all_event, NTP_READY_BIT);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }*/
    while (1)
    {
        ESP_LOGI(TAG, "hello");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}