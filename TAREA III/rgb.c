#include "rgb.h"
#include "driver/ledc.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include <string.h>

#define TAG "RGB"

/* ── GPIO mapping ───────────────────────────────────────────── */
#define RGB_GPIO_R   GPIO_NUM_12
#define RGB_GPIO_G   GPIO_NUM_7
#define RGB_GPIO_B   GPIO_NUM_10

/* ── LEDC ───────────────────────────────────────────────────── */
#define RGB_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define RGB_LEDC_TIMER    LEDC_TIMER_1
#define RGB_LEDC_FREQ_HZ  5000
#define RGB_LEDC_DUTY_RES LEDC_TIMER_8_BIT   /* 0-255 maps naturally */

#define CH_R  LEDC_CHANNEL_1
#define CH_G  LEDC_CHANNEL_2
#define CH_B  LEDC_CHANNEL_3

static rgb_config_t s_cfg = {0, 0, 0, 0};

static void set_channel(ledc_channel_t ch, uint8_t value, uint8_t brightness)
{
    uint32_t scaled = (uint32_t)value * brightness / 100;
    uint32_t duty = 255 - scaled;   /* Invertido para ánodo común */
    ledc_set_duty(RGB_LEDC_MODE, ch, duty);
    ledc_update_duty(RGB_LEDC_MODE, ch);
}

void rgb_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = RGB_LEDC_MODE,
        .duty_resolution = RGB_LEDC_DUTY_RES,
        .timer_num       = RGB_LEDC_TIMER,
        .freq_hz         = RGB_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channels[3] = {
        { .gpio_num=RGB_GPIO_R, .speed_mode=RGB_LEDC_MODE, .channel=CH_R,
          .timer_sel=RGB_LEDC_TIMER, .duty=0, .hpoint=0 },
        { .gpio_num=RGB_GPIO_G, .speed_mode=RGB_LEDC_MODE, .channel=CH_G,
          .timer_sel=RGB_LEDC_TIMER, .duty=0, .hpoint=0 },
        { .gpio_num=RGB_GPIO_B, .speed_mode=RGB_LEDC_MODE, .channel=CH_B,
          .timer_sel=RGB_LEDC_TIMER, .duty=0, .hpoint=0 },
    };
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&channels[i]));
    }

    ESP_LOGI(TAG, "RGB LED initialized (R=%d G=%d B=%d)",
             RGB_GPIO_R, RGB_GPIO_G, RGB_GPIO_B);
}

void rgb_set(const rgb_config_t *cfg)
{
    memcpy(&s_cfg, cfg, sizeof(rgb_config_t));
    set_channel(CH_R, cfg->red,   cfg->brightness);
    set_channel(CH_G, cfg->green, cfg->brightness);
    set_channel(CH_B, cfg->blue,  cfg->brightness);
    ESP_LOGI(TAG, "RGB set R=%d G=%d B=%d bright=%d%%",
             cfg->red, cfg->green, cfg->blue, cfg->brightness);
}

void rgb_get(rgb_config_t *cfg)
{
    memcpy(cfg, &s_cfg, sizeof(rgb_config_t));
}