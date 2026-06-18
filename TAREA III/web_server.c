#include "web_server.h"
#include "wifi_manager.h"
#include "rgb.h"
#include "servo.h"
#include "fan.h"
#include "ntc.h"
#include "ota.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

#define TAG "WEB"

/* ════════════════════════════════════════════════════════════════
   Dashboard HTML (stored in flash via string literal)
   ════════════════════════════════════════════════════════════════ */
static const char DASHBOARD_HTML[] =
"<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>STR 2026</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0;font-family:sans-serif}"
"body{background:#1a1a2e;color:#eee;padding:16px}"
"h1{text-align:center;margin-bottom:20px;color:#e94560}"
".card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:16px}"
"h2{color:#0f3460;background:#e94560;padding:6px 12px;border-radius:6px;margin-bottom:12px}"
"label{display:block;margin:6px 0 2px;font-size:.85rem;color:#aaa}"
"input,select{width:100%;padding:8px;border-radius:6px;border:none;"
"background:#0f3460;color:#eee;margin-bottom:8px}"
"button{background:#e94560;color:#fff;border:none;padding:10px 20px;"
"border-radius:6px;cursor:pointer;width:100%;margin-top:4px}"
"button:hover{background:#c73652}"
"#temp-display{font-size:2rem;text-align:center;padding:10px;color:#0ff}"
".grid2{display:grid;grid-template-columns:1fr 1fr;gap:8px}"
"</style></head><body>"
"<h1>STR 2026 – Control Ambiental</h1>"

/* ── Temperatura en vivo ── */
"<div class='card'><h2>🌡 Temperatura</h2>"
"<div id='temp-display'>--.-  °C</div></div>"

/* ── Fan ── */
"<div class='card'><h2>🌀 Ventilador</h2>"
"<label>Modo</label>"
"<select id='fan-mode'><option value='0'>Automático</option><option value='1'>Manual</option></select>"
"<div id='fan-auto-cfg'>"
"<label>Temperatura Deseada (°C)</label><input type='number' id='fan-desired' value='25' step='0.5'>"
"<label>Temperatura Máxima (°C)</label><input type='number' id='fan-max' value='35' step='0.5'>"
"</div>"
"<div id='fan-manual-cfg' style='display:none'>"
"<label>Velocidad Manual (%)</label><input type='number' id='fan-manual' value='0' min='0' max='100'>"
"</div>"
"<button onclick='setFan()'>Aplicar</button></div>"

/* ── Cortinas ── */
"<div class='card'><h2>🪟 Cortinas</h2>"
"<label>Modo</label>"
"<select id='servo-mode'><option value='0'>Manual</option><option value='1'>Programado</option></select>"
"<div id='servo-manual-cfg'>"
"<label>Apertura Manual (%)</label><input type='number' id='servo-manual' value='0' min='0' max='100'>"
"<button onclick='setServo()'>Aplicar</button>"
"</div>"
"<div id='servo-sched-cfg' style='display:none'>"
"<div id='sched-list'></div>"
"<button onclick='addSched()'>+ Agregar entrada</button>"
"<button onclick='saveSched()' style='margin-top:8px'>Guardar Horario</button>"
"</div></div>"

/* ── RGB ── */
"<div class='card'><h2>💡 Iluminación RGB</h2>"
"<div class='grid2'>"
"<div><label>Rojo</label><input type='number' id='rgb-r' value='0' min='0' max='255'></div>"
"<div><label>Verde</label><input type='number' id='rgb-g' value='0' min='0' max='255'></div>"
"<div><label>Azul</label><input type='number' id='rgb-b' value='0' min='0' max='255'></div>"
"<div><label>Brillo (%)</label><input type='number' id='rgb-bright' value='100' min='0' max='100'></div>"
"</div>"
"<button onclick='setRgb()'>Aplicar</button></div>"

/* ── Wi-Fi STA ── */
"<div class='card'><h2>📶 Wi-Fi (STA)</h2>"
"<label>SSID</label><input type='text' id='sta-ssid'>"
"<label>Contraseña</label><input type='password' id='sta-pass'>"
"<button onclick='setWifi(\"sta\")'>Conectar</button></div>"

/* ── Wi-Fi AP ── */
"<div class='card'><h2>📡 Punto de Acceso (AP)</h2>"
"<label>SSID</label><input type='text' id='ap-ssid'>"
"<label>Contraseña</label><input type='password' id='ap-pass'>"
"<button onclick='setWifi(\"ap\")'>Actualizar AP</button></div>"

/* ── OTA ── */
"<div class='card'><h2>🔄 Actualización OTA</h2>"
"<label>URL del firmware (.bin)</label><input type='text' id='ota-url' placeholder='http://192.168.x.x/firmware.bin'>"
"<button onclick='startOta()'>Actualizar Firmware</button></div>"

"<script>"
/* Poll temperatura */
"setInterval(async()=>{"
"const r=await fetch('/api/temperature');const d=await r.json();"
"document.getElementById('temp-display').textContent=d.celsius.toFixed(1)+' °C';"
"},2000);"

/* Fan mode toggle */
"document.getElementById('fan-mode').addEventListener('change',function(){"
"document.getElementById('fan-auto-cfg').style.display=this.value=='0'?'block':'none';"
"document.getElementById('fan-manual-cfg').style.display=this.value=='1'?'block':'none';"
"});"

/* Servo mode toggle */
"document.getElementById('servo-mode').addEventListener('change',function(){"
"document.getElementById('servo-manual-cfg').style.display=this.value=='0'?'block':'none';"
"document.getElementById('servo-sched-cfg').style.display=this.value=='1'?'block':'none';"
"});"

/* Set fan */
"async function setFan(){"
"const mode=parseInt(document.getElementById('fan-mode').value);"
"const body={mode,manual_percent:parseInt(document.getElementById('fan-manual').value)||0,"
"desired_temp:parseFloat(document.getElementById('fan-desired').value)||25,"
"max_temp:parseFloat(document.getElementById('fan-max').value)||35};"
"await fetch('/api/fan',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}"

/* Set servo manual */
"async function setServo(){"
"const body={mode:0,manual_percent:parseInt(document.getElementById('servo-manual').value)||0,schedule:[]};"
"await fetch('/api/servo',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}"

/* Schedule helpers */
"let schedEntries=[];"
"function addSched(){"
"const idx=schedEntries.length;"
"schedEntries.push({hour:0,percent:0,enabled:true});"
"const div=document.getElementById('sched-list');"
"div.innerHTML+=`<div class='grid2' style='margin-bottom:6px'>"
"<div><label>Hora</label><input type='number' id='sh${idx}' value='0' min='0' max='23'></div>"
"<div><label>%</label><input type='number' id='sp${idx}' value='0' min='0' max='100'></div>"
"</div>`;}"
"async function saveSched(){"
"const sched=schedEntries.map((_,i)=>({hour:parseInt(document.getElementById('sh'+i).value)||0,"
"percent:parseInt(document.getElementById('sp'+i).value)||0,enabled:true}));"
"const body={mode:1,manual_percent:0,schedule:sched};"
"await fetch('/api/servo',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}"

/* Set RGB */
"async function setRgb(){"
"const body={red:parseInt(document.getElementById('rgb-r').value)||0,"
"green:parseInt(document.getElementById('rgb-g').value)||0,"
"blue:parseInt(document.getElementById('rgb-b').value)||0,"
"brightness:parseInt(document.getElementById('rgb-bright').value)||100};"
"await fetch('/api/rgb',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}"

/* Wi-Fi */
"async function setWifi(type){"
"const ssid=document.getElementById(type+'-ssid').value;"
"const password=document.getElementById(type+'-pass').value;"
"await fetch('/api/wifi/'+type,{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid,password})});}"

/* OTA */
"async function startOta(){"
"const url=document.getElementById('ota-url').value;"
"if(!url)return alert('Ingresa una URL');"
"if(!confirm('¿Iniciar actualización OTA?'))return;"
"await fetch('/api/ota',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({url})});"
"alert('Actualizando… el dispositivo se reiniciará.');}"
"</script></body></html>";

/* ════════════════════════════════════════════════════════════════
   Helpers
   ════════════════════════════════════════════════════════════════ */
static esp_err_t read_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    int remaining = req->content_len;
    if (remaining >= (int)buf_size) return ESP_FAIL;
    int received = httpd_req_recv(req, buf, remaining);
    if (received <= 0) return ESP_FAIL;
    buf[received] = '\0';
    return ESP_OK;
}

static void send_json(httpd_req_t *req, cJSON *root)
{
    char *str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, str);
    free(str);
    cJSON_Delete(root);
}

static void send_ok(httpd_req_t *req)
{
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "status", "ok");
    send_json(req, j);
}

/* ════════════════════════════════════════════════════════════════
   Handlers
   ════════════════════════════════════════════════════════════════ */

/* GET / → dashboard */
static esp_err_t h_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, DASHBOARD_HTML);
    return ESP_OK;
}

/* GET /api/temperature */
static esp_err_t h_get_temp(httpd_req_t *req)
{
    float t = ntc_read_celsius();
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "celsius", (double)t);
    cJSON_AddBoolToObject(j, "alarm", fan_alarm_active());
    send_json(req, j);
    return ESP_OK;
}

/* POST /api/fan  { mode, manual_percent, desired_temp, max_temp } */
static esp_err_t h_post_fan(httpd_req_t *req)
{
    char buf[256];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body");
        return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(buf);
    if (!j) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_FAIL; }

    fan_config_t cfg;
    fan_get_config(&cfg);

    cJSON *mode = cJSON_GetObjectItem(j, "mode");
    cJSON *mp   = cJSON_GetObjectItem(j, "manual_percent");
    cJSON *dt   = cJSON_GetObjectItem(j, "desired_temp");
    cJSON *mt   = cJSON_GetObjectItem(j, "max_temp");

    if (cJSON_IsNumber(mode)) cfg.mode            = (fan_mode_t)mode->valueint;
    if (cJSON_IsNumber(mp))   cfg.manual_percent  = (uint8_t)mp->valueint;
    if (cJSON_IsNumber(dt))   cfg.desired_temp    = (float)dt->valuedouble;
    if (cJSON_IsNumber(mt))   cfg.max_temp        = (float)mt->valuedouble;
    cJSON_Delete(j);

    fan_set_config(&cfg);
    send_ok(req);
    return ESP_OK;
}

/* GET /api/fan */
static esp_err_t h_get_fan(httpd_req_t *req)
{
    fan_config_t cfg;
    fan_get_config(&cfg);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "mode",            cfg.mode);
    cJSON_AddNumberToObject(j, "manual_percent",  cfg.manual_percent);
    cJSON_AddNumberToObject(j, "desired_temp",    cfg.desired_temp);
    cJSON_AddNumberToObject(j, "max_temp",        cfg.max_temp);
    send_json(req, j);
    return ESP_OK;
}

/* POST /api/servo  { mode, manual_percent, schedule:[{hour,percent,enabled}] } */
static esp_err_t h_post_servo(httpd_req_t *req)
{
    char buf[512];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body"); return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(buf);
    if (!j) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_FAIL; }

    servo_config_t cfg;
    servo_get_config(&cfg);

    cJSON *mode = cJSON_GetObjectItem(j, "mode");
    cJSON *mp   = cJSON_GetObjectItem(j, "manual_percent");
    cJSON *sched = cJSON_GetObjectItem(j, "schedule");

    if (cJSON_IsNumber(mode)) cfg.mode           = (servo_mode_t)mode->valueint;
    if (cJSON_IsNumber(mp))   cfg.manual_percent = (uint8_t)mp->valueint;

    if (cJSON_IsArray(sched)) {
        memset(cfg.schedule, 0, sizeof(cfg.schedule));
        int idx = 0;
        cJSON *entry;
        cJSON_ArrayForEach(entry, sched) {
            if (idx >= SERVO_SCHEDULE_MAX) break;
            cJSON *h = cJSON_GetObjectItem(entry, "hour");
            cJSON *p = cJSON_GetObjectItem(entry, "percent");
            cJSON *e = cJSON_GetObjectItem(entry, "enabled");
            if (cJSON_IsNumber(h)) cfg.schedule[idx].hour    = (uint8_t)h->valueint;
            if (cJSON_IsNumber(p)) cfg.schedule[idx].percent = (uint8_t)p->valueint;
            cfg.schedule[idx].enabled = cJSON_IsTrue(e) || (!cJSON_IsFalse(e) && !cJSON_IsNull(e));
            idx++;
        }
    }
    cJSON_Delete(j);

    servo_set_config(&cfg);
    send_ok(req);
    return ESP_OK;
}

/* GET /api/servo */
static esp_err_t h_get_servo(httpd_req_t *req)
{
    servo_config_t cfg;
    servo_get_config(&cfg);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "mode", cfg.mode);
    cJSON_AddNumberToObject(j, "manual_percent", cfg.manual_percent);
    cJSON *arr = cJSON_AddArrayToObject(j, "schedule");
    for (int i = 0; i < SERVO_SCHEDULE_MAX; i++) {
        cJSON *e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "hour",    cfg.schedule[i].hour);
        cJSON_AddNumberToObject(e, "percent", cfg.schedule[i].percent);
        cJSON_AddBoolToObject(e, "enabled",   cfg.schedule[i].enabled);
        cJSON_AddItemToArray(arr, e);
    }
    send_json(req, j);
    return ESP_OK;
}

/* POST /api/rgb  { red, green, blue, brightness } */
static esp_err_t h_post_rgb(httpd_req_t *req)
{
    char buf[128];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body"); return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(buf);
    if (!j) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_FAIL; }

    rgb_config_t cfg;
    rgb_get(&cfg);

    cJSON *r  = cJSON_GetObjectItem(j, "red");
    cJSON *g  = cJSON_GetObjectItem(j, "green");
    cJSON *b  = cJSON_GetObjectItem(j, "blue");
    cJSON *br = cJSON_GetObjectItem(j, "brightness");

    if (cJSON_IsNumber(r))  cfg.red        = (uint8_t)r->valueint;
    if (cJSON_IsNumber(g))  cfg.green      = (uint8_t)g->valueint;
    if (cJSON_IsNumber(b))  cfg.blue       = (uint8_t)b->valueint;
    if (cJSON_IsNumber(br)) cfg.brightness = (uint8_t)br->valueint;
    cJSON_Delete(j);

    rgb_set(&cfg);
    send_ok(req);
    return ESP_OK;
}

/* GET /api/rgb */
static esp_err_t h_get_rgb(httpd_req_t *req)
{
    rgb_config_t cfg;
    rgb_get(&cfg);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "red",        cfg.red);
    cJSON_AddNumberToObject(j, "green",      cfg.green);
    cJSON_AddNumberToObject(j, "blue",       cfg.blue);
    cJSON_AddNumberToObject(j, "brightness", cfg.brightness);
    send_json(req, j);
    return ESP_OK;
}

/* POST /api/wifi/sta  { ssid, password } */
static esp_err_t h_post_wifi_sta(httpd_req_t *req)
{
    char buf[256];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body"); return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(buf);
    if (!j) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_FAIL; }

    wifi_creds_t creds = {0};
    cJSON *ssid = cJSON_GetObjectItem(j, "ssid");
    cJSON *pass = cJSON_GetObjectItem(j, "password");
    if (cJSON_IsString(ssid)) strncpy(creds.ssid,     ssid->valuestring, WIFI_SSID_MAX_LEN - 1);
    if (cJSON_IsString(pass)) strncpy(creds.password, pass->valuestring, WIFI_PASS_MAX_LEN - 1);
    cJSON_Delete(j);

    wifi_manager_set_sta(&creds);
    send_ok(req);
    return ESP_OK;
}

/* POST /api/wifi/ap  { ssid, password } */
static esp_err_t h_post_wifi_ap(httpd_req_t *req)
{
    char buf[256];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body"); return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(buf);
    if (!j) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_FAIL; }

    wifi_creds_t creds = {0};
    cJSON *ssid = cJSON_GetObjectItem(j, "ssid");
    cJSON *pass = cJSON_GetObjectItem(j, "password");
    if (cJSON_IsString(ssid)) strncpy(creds.ssid,     ssid->valuestring, WIFI_SSID_MAX_LEN - 1);
    if (cJSON_IsString(pass)) strncpy(creds.password, pass->valuestring, WIFI_PASS_MAX_LEN - 1);
    cJSON_Delete(j);

    wifi_manager_set_ap(&creds);
    send_ok(req);
    return ESP_OK;
}

/* GET /api/wifi */
static esp_err_t h_get_wifi(httpd_req_t *req)
{
    wifi_creds_t sta, ap;
    wifi_manager_get_sta(&sta);
    wifi_manager_get_ap(&ap);
    cJSON *j = cJSON_CreateObject();
    cJSON *j_sta = cJSON_AddObjectToObject(j, "sta");
    cJSON_AddStringToObject(j_sta, "ssid", sta.ssid);
    cJSON_AddBoolToObject(j, "connected", wifi_manager_is_connected());
    cJSON *j_ap = cJSON_AddObjectToObject(j, "ap");
    cJSON_AddStringToObject(j_ap, "ssid", ap.ssid);
    send_json(req, j);
    return ESP_OK;
}

/* POST /api/ota  { url } */
static esp_err_t h_post_ota(httpd_req_t *req)
{
    char buf[256];
    if (read_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body"); return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(buf);
    if (!j) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_FAIL; }

    cJSON *url = cJSON_GetObjectItem(j, "url");
    if (!cJSON_IsString(url)) {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing url");
        return ESP_FAIL;
    }

    /* Acknowledge before blocking */
    send_ok(req);
    ota_update(url->valuestring);   /* Reboots on success */
    cJSON_Delete(j);
    return ESP_OK;
}

/* ════════════════════════════════════════════════════════════════
   URI table
   ════════════════════════════════════════════════════════════════ */
static const httpd_uri_t URI_TABLE[] = {
    { .uri="/",              .method=HTTP_GET,  .handler=h_root        },
    { .uri="/api/temperature",.method=HTTP_GET, .handler=h_get_temp    },
    { .uri="/api/fan",       .method=HTTP_GET,  .handler=h_get_fan     },
    { .uri="/api/fan",       .method=HTTP_POST, .handler=h_post_fan    },
    { .uri="/api/servo",     .method=HTTP_GET,  .handler=h_get_servo   },
    { .uri="/api/servo",     .method=HTTP_POST, .handler=h_post_servo  },
    { .uri="/api/rgb",       .method=HTTP_GET,  .handler=h_get_rgb     },
    { .uri="/api/rgb",       .method=HTTP_POST, .handler=h_post_rgb    },
    { .uri="/api/wifi",      .method=HTTP_GET,  .handler=h_get_wifi    },
    { .uri="/api/wifi/sta",  .method=HTTP_POST, .handler=h_post_wifi_sta},
    { .uri="/api/wifi/ap",   .method=HTTP_POST, .handler=h_post_wifi_ap},
    { .uri="/api/ota",       .method=HTTP_POST, .handler=h_post_ota    },
};

void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 16;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    for (int i = 0; i < sizeof(URI_TABLE)/sizeof(URI_TABLE[0]); i++) {
        httpd_register_uri_handler(server, &URI_TABLE[i]);
    }
    ESP_LOGI(TAG, "HTTP server started on port %d", cfg.server_port);
}