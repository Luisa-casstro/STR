#include "ntc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include <math.h>

#define TAG "NTC"

/* ── Hardware ─────────────────────────────────────────────── */
#define NTC_ADC_UNIT      ADC_UNIT_1
#define NTC_ADC_CHANNEL   ADC_CHANNEL_2   /* GPIO2 on ESP32-C6 */
#define NTC_ADC_ATTEN     ADC_ATTEN_DB_12

/* ── Steinhart-Hart parameters (10 kΩ NTC, β = 3950) ──────── */
#define NTC_R_SERIES      10000.0f   /* Series resistor (Ω)       */
#define NTC_R_NOMINAL     10000.0f   /* NTC resistance at 25 °C   */
#define NTC_T_NOMINAL     298.15f    /* 25 °C in Kelvin            */
#define NTC_BETA          4950.0f
#define NTC_VCC           3300       /* mV                         */

static adc_oneshot_unit_handle_t s_adc_handle;

void ntc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = NTC_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = NTC_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, NTC_ADC_CHANNEL, &chan_cfg));

    ESP_LOGI(TAG, "NTC initialized on ADC1 channel %d", NTC_ADC_CHANNEL);
}

float ntc_read_celsius(void)
{
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, NTC_ADC_CHANNEL, &raw));

    /* Convert raw ADC → voltage (mV) → resistance */
    float voltage  = (raw * NTC_VCC) / 4095.0f;
    float r_ntc    = NTC_R_SERIES * voltage / (NTC_VCC - voltage);

    /* Steinhart-Hart (simplified β equation) */
    float steinhart = logf(r_ntc / NTC_R_NOMINAL) / NTC_BETA;
    steinhart      += 1.0f / NTC_T_NOMINAL;
    float celsius   = (1.0f / steinhart) - 273.15f; /* Calibration offset */

    return celsius;
}