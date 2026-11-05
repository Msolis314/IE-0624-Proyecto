/*
 * red.c
 *
 * Punto de acceso WiFi y arranque del servidor.
 * La dirección de la ESP32 en su propia red es 192.168.4.1.
 */

#include "red.h"

#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "servidor.h"

static const char *TAG = "red";

static void al_evento_wifi(void *arg, esp_event_base_t base, int32_t id, void *datos)
{
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *ev = datos;
        ESP_LOGI(TAG, "dispositivo conectado " MACSTR " (aid %d)", MAC2STR(ev->mac), ev->aid);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *ev = datos;
        ESP_LOGI(TAG, "dispositivo desconectado " MACSTR " (aid %d, razón %d)",
                 MAC2STR(ev->mac), ev->aid, ev->reason);
    }
}

static esp_err_t iniciar_punto_de_acceso(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init");

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {   /* ya creado: no es error */
        ESP_LOGE(TAG, "no se pudo crear el lazo de eventos");
        return err;
    }

    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_ap() != NULL, ESP_FAIL, TAG,
                        "no se pudo crear la interfaz del AP");

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "esp_wifi_init");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                            al_evento_wifi, NULL, NULL),
                        TAG, "registro de eventos WiFi");

    wifi_config_t cfg = { 0 };
    strlcpy((char *)cfg.ap.ssid, CONFIG_JUNIOR_WIFI_SSID, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len       = (uint8_t)strlen((const char *)cfg.ap.ssid);
    strlcpy((char *)cfg.ap.password, CONFIG_JUNIOR_WIFI_PASSWORD, sizeof(cfg.ap.password));
    cfg.ap.channel        = CONFIG_JUNIOR_WIFI_CANAL;
    cfg.ap.max_connection = CONFIG_JUNIOR_WIFI_MAX_CLIENTES;
    cfg.ap.pmf_cfg.required = false;

    if (strlen(CONFIG_JUNIOR_WIFI_PASSWORD) >= 8) {
        cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGW(TAG, "contraseña de menos de 8 caracteres: la red queda ABIERTA");
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "modo AP");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &cfg), TAG, "configuración del AP");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start");

    /* Sin ahorro de energía: menor latencia en los comandos */
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "red \"%s\" lista, canal %d. Abrir http://192.168.4.1",
             CONFIG_JUNIOR_WIFI_SSID, CONFIG_JUNIOR_WIFI_CANAL);
    return ESP_OK;
}

esp_err_t red_iniciar(void)
{
    ESP_RETURN_ON_ERROR(iniciar_punto_de_acceso(), TAG, "punto de acceso");
    ESP_RETURN_ON_ERROR(servidor_iniciar(), TAG, "servidor");
    return ESP_OK;
}
