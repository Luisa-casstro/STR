#include "servo.h"
#include "driver/ledc.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include <string.h>

#define TAG "SERVO"

/* ── Hardware ────────────────────────────────────────────────── */
#define SERVO_GPIO        GPIO_NUM_8

/* ── LEDC ────────────────────────────────────────────────────── */
#define SERVO_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_TIMER    LEDC_TIMER_2
#define SERVO_LEDC_CHANNEL  LEDC_CHANNEL_4
#define SERVO_LEDC_FREQ_HZ  50             /* 50 Hz – standard servo */
#define SERVO_LEDC_DUTY_RES LEDC_TIMER_14_BIT

/* ── Servo pulse timing (µs) ─────────────────────────────────── */
#define SERVO_US_MIN  500    /* 0 %   → 0.5 ms */
#define SERVO_US_MAX  2500   /* 100 % → 2.5 ms */

/* Period in µs at 50 Hz = 20000 µs */
#define SERVO_PERIOD_US  20000

static servo_config_t s_cfg = {
    .mode           = SERVO_MODE_MANUAL,
    .manual_percent = 0,
};

/* ── Helpers ─────────────────────────────────────────────────── */
static void apply_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t us        = SERVO_US_MIN + (SERVO_US_MAX - SERVO_US_MIN) * percent / 100;
    uint32_t max_duty  = (1 << SERVO_LEDC_DUTY_RES) - 1;
    uint32_t duty      = (us * max_duty) / SERVO_PERIOD_US;
    ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty);
    ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);
    ESP_LOGI(TAG, "Servo → %d%% (duty=%lu)", percent, (unsigned long)duty);
}

/* ── Public API ──────────────────────────────────────────────── */
void servo_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = SERVO_LEDC_MODE,
        .duty_resolution = SERVO_LEDC_DUTY_RES,
        .timer_num       = SERVO_LEDC_TIMER,
        .freq_hz         = SERVO_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num   = SERVO_GPIO,
        .speed_mode = SERVO_LEDC_MODE,
        .channel    = SERVO_LEDC_CHANNEL,
        .timer_sel  = SERVO_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    apply_percent(0);   /* Start closed */
    ESP_LOGI(TAG, "Servo initialized on GPIO%d", SERVO_GPIO);
}

void servo_set_percent(uint8_t percent)
{
    apply_percent(percent);
}

void servo_set_config(const servo_config_t *cfg)
{
    memcpy(&s_cfg, cfg, sizeof(servo_config_t));
    if (cfg->mode == SERVO_MODE_MANUAL) {
        apply_percent(cfg->manual_percent);
    }
    ESP_LOGI(TAG, "Config updated: mode=%d manual=%d%%", cfg->mode, cfg->manual_percent);
}

void servo_get_config(servo_config_t *cfg)
{
    memcpy(cfg, &s_cfg, sizeof(servo_config_t));
}

void servo_tick(uint8_t current_hour)
{
    if (s_cfg.mode != SERVO_MODE_SCHEDULED) return;

    for (int i = 0; i < SERVO_SCHEDULE_MAX; i++) {
        if (s_cfg.schedule[i].enabled &&
            s_cfg.schedule[i].hour == current_hour) {
            ESP_LOGI(TAG, "Schedule[%d] fired at hour %d → %d%%",
                     i, current_hour, s_cfg.schedule[i].percent);
            apply_percent(s_cfg.schedule[i].percent);
            break;  /* Only one entry per hour */
        }
    }
}