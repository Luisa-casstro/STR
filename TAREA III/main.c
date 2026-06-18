#include "wifi_manager.h"
#include "rgb.h"
#include "servo.h"
#include "fan.h"
#include "ntc.h"
#include "web_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sntp.h"
#include "time.h"

#define TAG "MAIN"

/* ── Sensor + control loop (every 2 s) ──────────────────────── */
static void sensor_task(void *arg)
{
    while (1) {
        float temp = ntc_read_celsius();
        ESP_LOGI(TAG, "Temperature: %.2f °C", temp);
        fan_tick(temp);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ── Scheduler tick (every 60 s) ────────────────────────────── */
static void scheduler_task(void *arg)
{
    while (1) {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        servo_tick((uint8_t)t.tm_hour);
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void app_main(void)
{
    /* 1. Wi-Fi (also initialises NVS flash) */
    wifi_manager_init();

    /* 2. Peripherals */
    rgb_init();
    servo_init();
    fan_init();
    ntc_init();

    /* 3. SNTP – needed for schedule timestamps */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    /* 4. Web server */
    web_server_start();

    /* 5. Background tasks */
    xTaskCreate(sensor_task,    "sensor",    4096, NULL, 5, NULL);
    xTaskCreate(scheduler_task, "scheduler", 2048, NULL, 4, NULL);

    ESP_LOGI(TAG, "STR 2026 running");
}