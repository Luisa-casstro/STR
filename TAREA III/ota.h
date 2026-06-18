#pragma once
#include "esp_err.h"

/**
 * @brief Download and apply a firmware image from `url`, then reboot.
 *        Returns ESP_FAIL if the update fails (no reboot in that case).
 */
esp_err_t ota_update(const char *url);