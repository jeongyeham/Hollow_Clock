idf_component_register(SRCS "main.c" "FreeRTOS_task.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_event step_motor wpa_supplicant nvs_flash esp_wifi)
