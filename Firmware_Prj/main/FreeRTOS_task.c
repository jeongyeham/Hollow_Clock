/*
* SPDX-FileCopyrightText: Copyright 2025 JeongYeham
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "main.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "step_motor.h"
#include "esp_log.h"

#define MOTOR_TAG "STEP_MOTOR"

#define STEPS_PER_REV 4096
QueueHandle_t motor_cmd_queue;
extern volatile int remaining_steps;

void step_motor_task(void* param)
{
    ESP_LOGI(MOTOR_TAG, "Stepper task started");
    motor_cmd_queue = xQueueCreate(4, sizeof(stepper_cmd_t));
    ESP_LOGI(MOTOR_TAG, "Stepper task queue is ready");
    while (1)
    {
        stepper_cmd_t cmd;
        if (xQueueReceive(motor_cmd_queue, &cmd, portMAX_DELAY))
        {
            ESP_LOGI(MOTOR_TAG, "New command: steps=%d, dir=%s, speed=%dus",
                     cmd.steps, cmd.dir_cw ? "CW" : "CCW", cmd.speed_us);
            stepper_set_motion(cmd.steps, cmd.dir_cw, cmd.speed_us);

            while (remaining_steps > 0)
            {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}

void stepper_rotate_angle(float degree, bool cw, int rpm)
{
    int total_steps = (int)(degree / 360.0f * STEPS_PER_REV);
    int us_per_step = (int)(60.0f * 1000000 / (rpm * STEPS_PER_REV));

    stepper_cmd_t cmd = {
        .steps = total_steps,
        .dir_cw = cw,
        .speed_us = us_per_step,
    };
    xQueueSend(motor_cmd_queue, &cmd, portMAX_DELAY);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"

#define CONNECTED_BIT      BIT0
#define ESP_TOUCH_DONE_BIT  BIT1
#define ESP_NVS_STORED_BIT BIT2
#define NTP_READY_BIT      BIT4

/*NVS_FLASH Configuration */
static const char* NVS_Name_space = "wifi_data";
static const char* NVS_Key = "key_wifi_data";
static nvs_handle_t wifi_nvs_handle;
static wifi_config_t wifi_config_stored;

void smart_config_task(void* pvParameters);

static const char* SMART_TAG = "smartconfig";

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    user_data_t* signal = (user_data_t*)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        EventBits_t uxBits_NVS = xEventGroupWaitBits(signal->all_event, ESP_NVS_STORED_BIT, true, false, (TickType_t)0);
        if (!(uxBits_NVS & (ESP_NVS_STORED_BIT)))
        {
            xTaskCreate(smart_config_task, "smart_config_task", 4096, signal, 3, NULL);
            memset(&wifi_config_stored, 0x0, sizeof(wifi_config_stored));
        }
        else
        {
            esp_wifi_connect();
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_ERROR_CHECK(nvs_open(NVS_Name_space, NVS_READWRITE, &wifi_nvs_handle));
        ESP_ERROR_CHECK(nvs_erase_key(wifi_nvs_handle, NVS_Key));
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(nvs_erase_all(wifi_nvs_handle));
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(nvs_commit(wifi_nvs_handle));
        nvs_close(wifi_nvs_handle);
        esp_wifi_connect();
        xEventGroupClearBits(signal->all_event, CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        user_data_t* user_signal = (user_data_t*)arg;
        xEventGroupSetBits(user_signal->all_event, CONNECTED_BIT);
        xEventGroupSetBits(user_signal->all_event, CONNECTED_BIT);
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
    {
        ESP_LOGI(SMART_TAG, "Scan done");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
    {
        ESP_LOGI(SMART_TAG, "Found channel");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        ESP_LOGI(SMART_TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t* evt = (smartconfig_event_got_ssid_pswd_t*)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = {0};
        uint8_t password[65] = {0};

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true)
        {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));

        ESP_LOGI(SMART_TAG, "SSID:%s", ssid);
        ESP_LOGI(SMART_TAG, "PASSWORD:%s", password);

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(nvs_open(NVS_Name_space, NVS_READWRITE, &wifi_nvs_handle));
        ESP_ERROR_CHECK(nvs_set_blob(wifi_nvs_handle, NVS_Key, &wifi_config, sizeof(wifi_config)));
        ESP_ERROR_CHECK(nvs_commit(wifi_nvs_handle));
        nvs_close(wifi_nvs_handle);

        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        xEventGroupSetBits(signal->all_event, ESP_TOUCH_DONE_BIT);
    }
}

void initialise_wifi_task(void* pvParameters)
{
    user_data_t* signal = (user_data_t*)pvParameters;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(nvs_flash_init());


    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, signal));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, signal));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, signal));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(nvs_open(NVS_Name_space, NVS_READWRITE, &wifi_nvs_handle));

    uint32_t len = sizeof(wifi_config_stored);
    esp_err_t ret = nvs_get_blob(wifi_nvs_handle, NVS_Key, &wifi_config_stored, (size_t*)&len);

    if (likely(ret == ESP_OK))
    {
        xEventGroupSetBits(signal->all_event, ESP_NVS_STORED_BIT);
        nvs_close(wifi_nvs_handle);
        ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_config_stored));
    }
    else
    {
        xEventGroupClearBits(signal->all_event, ESP_NVS_STORED_BIT);
        nvs_close(wifi_nvs_handle);
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelete(NULL);
}

void smart_config_task(void* pvParameters)
{
    user_data_t* user_signal = (user_data_t*)pvParameters;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (1)
    {
        EventBits_t uxBits = xEventGroupWaitBits(user_signal->all_event, CONNECTED_BIT | ESP_TOUCH_DONE_BIT, true,
                                                 false, portMAX_DELAY);
        if (likely(uxBits & CONNECTED_BIT))
        {
            ESP_LOGI(SMART_TAG, "WiFi Connected to ap");
            xEventGroupSetBits(user_signal->all_event, CONNECTED_BIT);
        }
        if (likely(uxBits & ESP_TOUCH_DONE_BIT))
        {
            ESP_LOGI(SMART_TAG, "smart_config over");
            esp_smartconfig_stop();
            xEventGroupSetBits(user_signal->all_event, ESP_TOUCH_DONE_BIT);
            break;
        }
    }
    vTaskDelete(NULL);
}
