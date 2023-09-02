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

#ifndef _MAIN_H_
#define _MAIN_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "stdio.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <sys/param.h>
#include "esp_sntp.h"
#include "time.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"



    typedef struct
    {
        EventGroupHandle_t all_event;

    } user_data_t;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
#define CONNECTED_BIT      BIT0
#define ESPTOUCH_DONE_BIT  BIT1
#define ESP_NVS_STORED_BIT BIT2
#define NTP_READY_BIT      BIT4

#ifdef __cplusplus
}
#endif

#endif
