#include "wifi_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <string.h>

#define TAG "WIFI_MGR"

/* ── NVS keys ────────────────────────────────────────────────── */
#define NVS_NAMESPACE  "wifi_cfg"
#define NVS_KEY_STA_SSID  "sta_ssid"
#define NVS_KEY_STA_PASS  "sta_pass"
#define NVS_KEY_AP_SSID   "ap_ssid"
#define NVS_KEY_AP_PASS   "ap_pass"

/* ── Defaults ────────────────────────────────────────────────── */
#define DEFAULT_STA_SSID  "MyNetwork"
#define DEFAULT_STA_PASS  "MyPassword"
#define DEFAULT_AP_SSID   "STR2026-AP"
#define DEFAULT_AP_PASS   "str2026pass"

static wifi_creds_t s_sta = { DEFAULT_STA_SSID, DEFAULT_STA_PASS };
static wifi_creds_t s_ap  = { DEFAULT_AP_SSID,  DEFAULT_AP_PASS  };
static bool         s_connected = false;

/* ── NVS helpers ─────────────────────────────────────────────── */
static void nvs_load_str(const char *key, char *out, size_t max_len,
                          const char *fallback)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        size_t len = max_len;
        if (nvs_get_str(h, key, out, &len) != ESP_OK) {
            strncpy(out, fallback, max_len - 1);
            out[max_len - 1] = '\0';
        }
        nvs_close(h);
    } else {
        strncpy(out, fallback, max_len - 1);
        out[max_len - 1] = '\0';
    }
}

static void nvs_save_str(const char *key, const char *value)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, key, value);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* ── Event handler ───────────────────────────────────────────── */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        ESP_LOGW(TAG, "STA disconnected – retrying…");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
    }
}

/* ── Internal apply ──────────────────────────────────────────── */
static void apply_config(void)
{
    wifi_config_t sta_cfg = {0};
    wifi_config_t ap_cfg  = {0};

    strncpy((char *)sta_cfg.sta.ssid,     s_sta.ssid,     sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, s_sta.password, sizeof(sta_cfg.sta.password) - 1);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    strncpy((char *)ap_cfg.ap.ssid,     s_ap.ssid,     sizeof(ap_cfg.ap.ssid) - 1);
    strncpy((char *)ap_cfg.ap.password, s_ap.password, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.ssid_len   = strlen(s_ap.ssid);
    ap_cfg.ap.authmode   = WIFI_AUTH_WPA2_PSK;
    ap_cfg.ap.max_connection = 4;

    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,  &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
}

/* ── Public API ──────────────────────────────────────────────── */
void wifi_manager_init(void)
{
    /* NVS flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Load saved credentials */
    nvs_load_str(NVS_KEY_STA_SSID, s_sta.ssid,     sizeof(s_sta.ssid),     DEFAULT_STA_SSID);
    nvs_load_str(NVS_KEY_STA_PASS, s_sta.password,  sizeof(s_sta.password),  DEFAULT_STA_PASS);
    nvs_load_str(NVS_KEY_AP_SSID,  s_ap.ssid,       sizeof(s_ap.ssid),       DEFAULT_AP_SSID);
    nvs_load_str(NVS_KEY_AP_PASS,  s_ap.password,   sizeof(s_ap.password),   DEFAULT_AP_PASS);

    /* Network stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    apply_config();

    ESP_LOGI(TAG, "Wi-Fi initialized. STA→%s  AP→%s", s_sta.ssid, s_ap.ssid);
}

void wifi_manager_set_sta(const wifi_creds_t *creds)
{
    memcpy(&s_sta, creds, sizeof(wifi_creds_t));
    nvs_save_str(NVS_KEY_STA_SSID, s_sta.ssid);
    nvs_save_str(NVS_KEY_STA_PASS, s_sta.password);
    apply_config();
}

void wifi_manager_get_sta(wifi_creds_t *creds)
{
    memcpy(creds, &s_sta, sizeof(wifi_creds_t));
}

void wifi_manager_set_ap(const wifi_creds_t *creds)
{
    memcpy(&s_ap, creds, sizeof(wifi_creds_t));
    nvs_save_str(NVS_KEY_AP_SSID, s_ap.ssid);
    nvs_save_str(NVS_KEY_AP_PASS, s_ap.password);
    apply_config();
}

void wifi_manager_get_ap(wifi_creds_t *creds)
{
    memcpy(creds, &s_ap, sizeof(wifi_creds_t));
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}