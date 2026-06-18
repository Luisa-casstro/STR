#include "fan.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>

#define TAG "FAN"

/* ── GPIO ────────────────────────────────────────────────────── */
#define FAN_GPIO        GPIO_NUM_3
#define ALARM_LED_GPIO  GPIO_NUM_4

/* ── LEDC (fan PWM) ──────────────────────────────────────────── */
#define FAN_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define FAN_LEDC_TIMER    LEDC_TIMER_0
#define FAN_LEDC_CHANNEL  LEDC_CHANNEL_0
#define FAN_LEDC_FREQ_HZ  25000          /* 25 kHz – inaudible for most fans */
#define FAN_LEDC_DUTY_RES LEDC_TIMER_8_BIT

/* ── State ───────────────────────────────────────────────────── */
static fan_config_t  s_cfg = {
    .mode           = FAN_MODE_AUTO,
    .manual_percent = 0,
    .desired_temp   = 25.0f,
    .max_temp       = 35.0f,
};
static bool          s_alarm   = false;
static TimerHandle_t s_alarm_timer = NULL;
static bool          s_led_state   = false;

/* ── Alarm LED blink (1 Hz via software timer) ───────────────── */
static void alarm_blink_cb(TimerHandle_t xTimer)
{
    s_led_state = !s_led_state;
    gpio_set_level(ALARM_LED_GPIO, s_led_state ? 1 : 0);
}

static void alarm_start(void)
{
    if (s_alarm) return;
    s_alarm = true;
    xTimerStart(s_alarm_timer, 0);
    ESP_LOGW(TAG, "Alarm ON – temperature exceeded max");
}

static void alarm_stop(void)
{
    if (!s_alarm) return;
    s_alarm = false;
    xTimerStop(s_alarm_timer, 0);
    s_led_state = false;
    gpio_set_level(ALARM_LED_GPIO, 0);
    ESP_LOGI(TAG, "Alarm OFF");
}

/* ── Fan speed helper ────────────────────────────────────────── */
static void set_fan_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)percent * 255 / 100;
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, duty);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
    ESP_LOGI(TAG, "Fan speed → %d%%", percent);
}

/* ── Public API ──────────────────────────────────────────────── */
void fan_init(void)
{
    /* Fan PWM */
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = FAN_LEDC_MODE,
        .duty_resolution = FAN_LEDC_DUTY_RES,
        .timer_num       = FAN_LEDC_TIMER,
        .freq_hz         = FAN_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num   = FAN_GPIO,
        .speed_mode = FAN_LEDC_MODE,
        .channel    = FAN_LEDC_CHANNEL,
        .timer_sel  = FAN_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    /* Alarm LED */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << ALARM_LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));
    gpio_set_level(ALARM_LED_GPIO, 0);

    /* Blink timer: period = 500 ms → toggles at 1 Hz */
    s_alarm_timer = xTimerCreate("alarm_blink", pdMS_TO_TICKS(500),
                                  pdTRUE, NULL, alarm_blink_cb);

    set_fan_percent(0);
    ESP_LOGI(TAG, "Fan initialized (GPIO%d), alarm LED (GPIO%d)",
             FAN_GPIO, ALARM_LED_GPIO);
}

void fan_set_config(const fan_config_t *cfg)
{
    memcpy(&s_cfg, cfg, sizeof(fan_config_t));
    if (cfg->mode == FAN_MODE_MANUAL) {
        alarm_stop();
        set_fan_percent(cfg->manual_percent);
    }
    ESP_LOGI(TAG, "Config: mode=%d manual=%d%% desired=%.1f max=%.1f",
             cfg->mode, cfg->manual_percent, cfg->desired_temp, cfg->max_temp);
}

void fan_get_config(fan_config_t *cfg)
{
    memcpy(cfg, &s_cfg, sizeof(fan_config_t));
}

void fan_tick(float temp)
{
    if (s_cfg.mode != FAN_MODE_AUTO) return;

    float desired = s_cfg.desired_temp;
    float max     = s_cfg.max_temp;

    /* Alarm: temperature above maximum */
    if (temp > max) {
        alarm_start();
    } else {
        alarm_stop();
    }

    /* Proportional speed */
    uint8_t percent = 0;
    if (temp <= desired) {
        percent = 0;
    } else if (temp >= max) {
        percent = 100;
    } else {
        percent = (uint8_t)((temp - desired) * 100.0f / (max - desired));
    }
    set_fan_percent(percent);
}

bool fan_alarm_active(void)
{
    return s_alarm;
}