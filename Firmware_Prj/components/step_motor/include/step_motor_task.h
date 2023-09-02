#include <sys/cdefs.h>
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


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "driver/dedic_gpio.h"
#include "hal/dedic_gpio_cpu_ll.h"
#include "esp_log.h"

_Noreturn void step_motor_task(__attribute__((unused)) void *param);