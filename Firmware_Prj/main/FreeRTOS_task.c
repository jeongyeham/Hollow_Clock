/*
 * SPDX-FileCopyrightText: Copyright 2025 JeongYeham
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "main.h"
#include "FreeRTOS_task.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "esp_log.h"
#include "esp_sntp.h"
#include "step_motor.h"

#define MOTOR_TAG "STEP_MOTOR"
#define CLOCK_TAG  "CLOCK_TASK"

// 前向声明
void clock_control_task(void* pvParameters);
static void update_clock_time(user_data_t* user_data);
static float time_to_angle(int hour, int minute);
static void feed_watchdog_if_needed(user_data_t* user_data);

// 时钟控制处理函数 - 可以在管理任务空转时直接调用
void clock_control_handler(clock_control_handle_t* handle);

// 设置目标时间
void set_clock_target_time(clock_control_handle_t* handle, int hour, int minute);

// 获取时钟状态
clock_state_t get_clock_state(clock_control_handle_t* handle);

// 静态时钟控制句柄定义
static clock_control_handle_t clock_handle = {0};


void step_motor_task(void* pvParameters)
{
    user_data_t* signal = (user_data_t*)pvParameters;
    ESP_LOGI(MOTOR_TAG, "Stepper task started");
    signal->motor_control = stepper_driver_init();
    signal->motor_control->motor_cmd_queue = xQueueCreate(4, sizeof(stepper_cmd_t));
    ESP_LOGI(MOTOR_TAG, "Stepper task queue is ready");
    while (1)
    {

        stepper_cmd_t cmd;
        if unlikely (xQueueReceive(signal->motor_control->motor_cmd_queue, &cmd, portMAX_DELAY))
        {
            ESP_LOGI(MOTOR_TAG, "New command: steps=%d, dir=%s, speed=%dus", cmd.steps, cmd.dir_cw ? "CW" : "CCW",
                     cmd.speed_us);
            stepper_set_time(signal->motor_control, cmd.steps, cmd.dir_cw, cmd.speed_us);

            while (stepper_is_moving(signal->motor_control))
            {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            
            // 通知时钟任务电机运动完成
            xEventGroupSetBits(signal->all_event, CLOCK_MOVE_COMPLETE_BIT);
        }
    }
}

void stepper_rotate_angle(const motor_control_t* motor_control, float degree, bool cw, float rpm)
{
    int total_steps = (int)(degree / 360.0f * 4096.0f);
    int us_per_step = (int)(60.0f * 1000000 / (rpm * 4096.0f));

    stepper_cmd_t cmd = {
        .steps = total_steps,
        .dir_cw = cw,
        .speed_us = us_per_step,
    };
    xQueueSend(motor_control->motor_cmd_queue, &cmd, portMAX_DELAY);
}

// 删除重复的声明和错误的实现，因为该函数已经在step_motor.h中声明并定义

// 将时间转换为角度的函数
static float time_to_angle(int hour, int minute) 
{
    // 小时角度计算：每小时30度，每分钟0.5度
    float hour_angle = (hour % 12) * 30.0f + minute * 0.5f;
    return hour_angle;
}

// 更新时钟时间
static void update_clock_time(user_data_t* user_data) 
{
    // 获取当前系统时间
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    user_data->current_time.hour = timeinfo.tm_hour;
    user_data->current_time.minute = timeinfo.tm_min;
    user_data->current_time.second = timeinfo.tm_sec;
    
    ESP_LOGI(CLOCK_TAG, "Current time: %02d:%02d:%02d", 
             user_data->current_time.hour, 
             user_data->current_time.minute, 
             user_data->current_time.second);
}

// 根据需要喂狗
static void feed_watchdog_if_needed(user_data_t* user_data) 
{
    // 如果看门狗使能且时钟处于空闲状态，则执行喂狗操作
    if (user_data->watchdog_enabled && user_data->clock_state == CLOCK_STATE_IDLE) {
        ESP_LOGD(CLOCK_TAG, "Feeding watchdog...");
        // 这里添加实际的喂狗代码
        // 例如: esp_task_wdt_reset();
    }
}

// 时钟控制任务
void clock_control_task(void* pvParameters)
{
    user_data_t* user_data = (user_data_t*)pvParameters;
    user_data->clock_state = CLOCK_STATE_IDLE;
    user_data->watchdog_enabled = true; // 启用看门狗
    
    // 使用文件作用域静态句柄
    if (!clock_handle.initialized) {
        clock_handle.user_data = user_data;
        clock_handle.initialized = true;
    }
    
    // 初始化时间
    update_clock_time(user_data);
    
    TickType_t last_minute_check = xTaskGetTickCount();
    const TickType_t minute_check_interval = pdMS_TO_TICKS(1000); // 每秒检查一次时间
    
    while (1) {
        // 检查是否需要更新时间（每分钟）
        if (xTaskGetTickCount() - last_minute_check >= minute_check_interval) {
            last_minute_check = xTaskGetTickCount();
            
            // 更新当前时间
            clock_time_t old_time = user_data->current_time;
            update_clock_time(user_data);
            
            // 检查分钟是否变化（分针跳动）
            if (user_data->current_time.minute != old_time.minute || 
                user_data->current_time.hour != old_time.hour) {
                ESP_LOGI(CLOCK_TAG, "Minute changed, moving minute hand");
                
                // 计算需要旋转的角度
                float target_angle = time_to_angle(user_data->current_time.hour, user_data->current_time.minute);
                float current_angle = time_to_angle(old_time.hour, old_time.minute);
                float angle_diff = target_angle - current_angle;
                
                // 规范化角度差到 [-180, 180] 范围
                if (angle_diff > 180.0f) {
                    angle_diff -= 360.0f;
                } else if (angle_diff < -180.0f) {
                    angle_diff += 360.0f;
                }
                
                // 确定旋转方向
                bool dir_cw = angle_diff >= 0;
                float abs_angle_diff = dir_cw ? angle_diff : -angle_diff;
                
                ESP_LOGI(CLOCK_TAG, "Rotating minute hand by %.2f degrees %s", 
                         abs_angle_diff, dir_cw ? "clockwise" : "counter-clockwise");
                
                // 设置状态为运动状态
                user_data->clock_state = CLOCK_STATE_MOVING;
                
                // 发送旋转命令
                stepper_rotate_angle(user_data->motor_control, abs_angle_diff, dir_cw, 6.0f); // 6 RPM速度
                
                // 等待运动完成
                EventBits_t uxBits = xEventGroupWaitBits(user_data->all_event, 
                                                         CLOCK_MOVE_COMPLETE_BIT, 
                                                         pdTRUE, 
                                                         pdFALSE, 
                                                         portMAX_DELAY);
                
                if (uxBits & CLOCK_MOVE_COMPLETE_BIT) {
                    ESP_LOGI(CLOCK_TAG, "Minute hand movement completed");
                }
                
                // 恢复空闲状态
                user_data->clock_state = CLOCK_STATE_IDLE;
            }
        }
        
        // 调用时钟控制处理函数
        clock_control_handler(&clock_handle);
        
        // 短暂延迟以允许其他任务执行
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 时钟控制处理函数 - 可以在管理任务空转时直接调用
void clock_control_handler(clock_control_handle_t* handle)
{
    if (!handle || !handle->initialized || !handle->user_data) {
        return;
    }
    
    user_data_t* user_data = handle->user_data;
    
    // 检查是否有时间调整请求
    EventBits_t uxBits = xEventGroupGetBits(user_data->all_event);
    if (uxBits & CLOCK_ADJUST_TIME_BIT) {
        ESP_LOGI(CLOCK_TAG, "Time adjustment requested via handler");
        xEventGroupClearBits(user_data->all_event, CLOCK_ADJUST_TIME_BIT);
        
        // 设置状态为调整状态
        user_data->clock_state = CLOCK_STATE_ADJUSTING;
        
        // 计算目标角度和当前角度的差值
        float target_angle = time_to_angle(user_data->target_time.hour, user_data->target_time.minute);
        float current_angle = time_to_angle(user_data->current_time.hour, user_data->current_time.minute);
        float angle_diff = target_angle - current_angle;
        
        // 规范化角度差到 [-180, 180] 范围
        if (angle_diff > 180.0f) {
            angle_diff -= 360.0f;
        } else if (angle_diff < -180.0f) {
            angle_diff += 360.0f;
        }
        
        // 确定旋转方向
        bool dir_cw = angle_diff >= 0;
        float abs_angle_diff = dir_cw ? angle_diff : -angle_diff;
        
        ESP_LOGI(CLOCK_TAG, "Adjusting time: rotating by %.2f degrees %s", 
                 abs_angle_diff, dir_cw ? "clockwise" : "counter-clockwise");
        
        // 发送旋转命令
        stepper_rotate_angle(user_data->motor_control, abs_angle_diff, dir_cw, 10.0f); // 10 RPM速度
        
        // 等待运动完成
        EventBits_t uxBits = xEventGroupWaitBits(user_data->all_event, 
                                                 CLOCK_MOVE_COMPLETE_BIT, 
                                                 pdTRUE, 
                                                 pdFALSE, 
                                                 portMAX_DELAY);
        
        if (uxBits & CLOCK_MOVE_COMPLETE_BIT) {
            ESP_LOGI(CLOCK_TAG, "Time adjustment completed");
            // 更新当前时间
            user_data->current_time = user_data->target_time;
        }
        
        // 恢复空闲状态
        user_data->clock_state = CLOCK_STATE_IDLE;
    }
    
    // 喂狗操作
    feed_watchdog_if_needed(user_data);
}

// 设置目标时间
void set_clock_target_time(clock_control_handle_t* handle, int hour, int minute)
{
    if (!handle || !handle->initialized || !handle->user_data) {
        return;
    }
    
    user_data_t* user_data = handle->user_data;
    user_data->target_time.hour = hour;
    user_data->target_time.minute = minute;
    
    // 触发时间调整事件
    xEventGroupSetBits(user_data->all_event, CLOCK_ADJUST_TIME_BIT);
}

// 获取时钟状态
clock_state_t get_clock_state(clock_control_handle_t* handle)
{
    if (!handle || !handle->initialized || !handle->user_data) {
        return CLOCK_STATE_IDLE;
    }
    
    return handle->user_data->clock_state;
}

void motor_control_task(void* pvParameters)
{
    user_data_t* signal = (user_data_t*)pvParameters;
    
    // 示例：旋转90度，顺时针，10 RPM
    stepper_rotate_angle(signal->motor_control, 90, true, 10);
    vTaskDelay(pdMS_TO_TICKS(2000)); // 等待2秒
    
    // 示例：旋转2秒，逆时针，速度500us/step
    stepper_rotate_time(signal->motor_control, 2000, false, 500);
    vTaskDelay(pdMS_TO_TICKS(3000)); // 等待3秒
    
    // 示例：旋转到特定角度 (例如180度位置)
    stepper_rotate_to_angle(signal->motor_control, 180.0f, 15.0f);
    vTaskDelay(pdMS_TO_TICKS(2000)); // 等待2秒
    
    while (1)
    {
        // 每10秒执行一次示例动作
        ESP_LOGI("MOTOR_CTRL", "Performing periodic rotation");
        stepper_rotate_angle(signal->motor_control, 30, true, 5); // 旋转30度
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <time.h>
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#define CONNECTED_BIT BIT0
#define ESP_TOUCH_DONE_BIT BIT1
#define ESP_NVS_STORED_BIT BIT2
#define NTP_READY_BIT BIT4

/*NVS_FLASH Configuration */
static const char* NVS_Name_space = "wifi_data";
static const char* NVS_Key = "key_wifi_data";
static nvs_handle_t wifi_nvs_handle;
static wifi_config_t wifi_config_stored;

void smart_config_task(void* pvParameters);

static const char* SMART_TAG = "smartconfig";

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
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
        EventBits_t uxBits =
            xEventGroupWaitBits(user_signal->all_event, CONNECTED_BIT | ESP_TOUCH_DONE_BIT, true, false, portMAX_DELAY);
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
            ESP_LOGI("MAIN", "Network found, prepare to connect SNTP");
            setenv("TZ", "EST-8", 1);
            tzset();
            sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
            esp_sntp_setservername(0, "ntp1.aliyun.com");
            esp_sntp_setservername(1, "ntp2.aliyun.com");
            esp_sntp_setservername(2, "ntp3.aliyun.com");
            esp_sntp_init();
            xEventGroupSetBits(user_signal->all_event, NTP_READY_BIT);
            break;
        }
    }
    vTaskDelete(NULL);
}