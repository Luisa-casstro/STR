#pragma once
#include <stdbool.h>

#define WIFI_SSID_MAX_LEN  32
#define WIFI_PASS_MAX_LEN  64

/**
 * @brief Wi-Fi credentials structure.
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
} wifi_creds_t;

/**
 * @brief Initialize NVS, Wi-Fi stack, STA+AP mode.
 *        Loads saved credentials from NVS or uses defaults.
 */
void wifi_manager_init(void);

/**
 * @brief Update the Station (STA) credentials and reconnect.
 *        Credentials are saved to NVS.
 */
void wifi_manager_set_sta(const wifi_creds_t *creds);

/**
 * @brief Get current STA credentials.
 */
void wifi_manager_get_sta(wifi_creds_t *creds);

/**
 * @brief Update the Soft-AP credentials and restart AP.
 *        Credentials are saved to NVS.
 */
void wifi_manager_set_ap(const wifi_creds_t *creds);

/**
 * @brief Get current Soft-AP credentials.
 */
void wifi_manager_get_ap(wifi_creds_t *creds);

/**
 * @brief Returns true if STA is connected.
 */
bool wifi_manager_is_connected(void);