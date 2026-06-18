#include "ota.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define TAG "OTA"

esp_err_t ota_update(const char *url)
{
    ESP_LOGI(TAG, "Starting OTA from: %s", url);

    esp_http_client_config_t http_cfg = {
        .url            = url,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful – rebooting…");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    }
    return ret;
}