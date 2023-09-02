// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
// Copyright 2022 JeongYeham
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "smart_config.h"


/*NVS_FLASH Configuration */
static const char *NVS_Name_space = "wifi_data";
static const char *NVS_Key = "key_wifi_data";
static nvs_handle_t wifi_nvs_handle;
static wifi_config_t wifi_config_stored;



static const char *TAG = "smartconfig";

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    user_data_t *signal = (user_data_t *)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        EventBits_t uxBits_NVS = xEventGroupWaitBits(signal->all_event, ESP_NVS_STORED_BIT, pdFALSE, pdTRUE, (TickType_t)0);
        if (!(uxBits_NVS & (ESP_NVS_STORED_BIT)))
        {
            xTaskCreate(smartconfig_task, "smartconfig_task", 4096, signal, 3, NULL);
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
        user_data_t *signal = (user_data_t *)arg;
        xEventGroupSetBits(signal->all_event, CONNECTED_BIT);
        xEventGroupSetBits(signal->all_event, CONNECTED_BIT);
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
    {
        ESP_LOGI(TAG, "Scan done");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
    {
        ESP_LOGI(TAG, "Found channel");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
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

        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(nvs_open(NVS_Name_space, NVS_READWRITE, &wifi_nvs_handle));
        ESP_ERROR_CHECK(nvs_set_blob(wifi_nvs_handle, NVS_Key, &wifi_config, sizeof(wifi_config)));
        ESP_ERROR_CHECK(nvs_commit(wifi_nvs_handle));
        nvs_close(wifi_nvs_handle);

        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        xEventGroupSetBits(signal->all_event, ESPTOUCH_DONE_BIT);
    }
}

void initialise_wifi_task(void *pvParameters)
{
    user_data_t *signal = (user_data_t *)pvParameters;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(nvs_flash_init());


    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, signal));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, signal));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, signal));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(nvs_open(NVS_Name_space, NVS_READWRITE, &wifi_nvs_handle));

    uint32_t len = sizeof(wifi_config_stored);
    esp_err_t ret = nvs_get_blob(wifi_nvs_handle, NVS_Key, &wifi_config_stored, (size_t *)&len);

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

void smartconfig_task(void *pvParameters)
{
    user_data_t *signal = (user_data_t *)pvParameters;
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT()
        ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (1)
    {
        uxBits = xEventGroupWaitBits(signal->all_event, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (likely(uxBits & CONNECTED_BIT))
        {
            ESP_LOGI(TAG, "WiFi Connected to ap");
            xEventGroupSetBits(signal->all_event, CONNECTED_BIT);
        }
        if (likely(uxBits & ESPTOUCH_DONE_BIT))
        {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            xEventGroupSetBits(signal->all_event, ESPTOUCH_DONE_BIT);
            break;
        }
    }
    vTaskDelete(NULL);
}
