/*
 * servidor.c
 *
 * - GET /, /app.js, /estilo.css: archivos de web/ embebidos en el firmware.
 * - /ws: WebSocket. Cada mensaje de texto se traduce con protocolo.c y se
 *   envía como comando_t. Al cerrarse, se avisa con CMD_CLIENTE_PERDIDO.
 * - Una tarea pide al servidor, cada CONFIG_JUNIOR_TELEMETRIA_MS, que envíe
 *   el estado a todos los WebSockets (con httpd_queue_work, para que los
 *   envíos ocurran dentro de la tarea del servidor).
 */

#include "servidor.h"

#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "estado.h"
#include "protocolo.h"

#if !CONFIG_HTTPD_WS_SUPPORT
#error "Falta CONFIG_HTTPD_WS_SUPPORT=y en sdkconfig.defaults (rm -f sdkconfig && make set-target)"
#endif

static const char *TAG = "servidor";

#define MAX_SOCKETS            4     /* conexiones HTTP/WS abiertas a la vez */
#define LARGO_MAX_MENSAJE      32    /* el mensaje más largo es "MODO MANUAL" */
#define LARGO_JSON             320
#define TELEMETRIA_PILA_BYTES  3072
#define TELEMETRIA_PRIORIDAD   4     /* menor que control (10) */

/* Archivos de web/ embebidos (EMBED_TXTFILES agrega un '\0' al final) */
extern const char index_html_inicio[] asm("_binary_index_html_start");
extern const char index_html_fin[]    asm("_binary_index_html_end");
extern const char app_js_inicio[]     asm("_binary_app_js_start");
extern const char app_js_fin[]        asm("_binary_app_js_end");
extern const char estilo_css_inicio[] asm("_binary_estilo_css_start");
extern const char estilo_css_fin[]    asm("_binary_estilo_css_end");

static httpd_handle_t s_servidor = NULL;
static atomic_bool    s_envio_pendiente = false;

// Archivos estáticos embebidos

static esp_err_t enviar_embebido(httpd_req_t *req, const char *inicio,
                                 const char *fin, const char *tipo)
{
    httpd_resp_set_type(req, tipo);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, inicio, fin - inicio - 1);
}

static esp_err_t get_index(httpd_req_t *req)
{
    return enviar_embebido(req, index_html_inicio, index_html_fin, "text/html; charset=utf-8");
}

static esp_err_t get_app_js(httpd_req_t *req)
{
    return enviar_embebido(req, app_js_inicio, app_js_fin, "application/javascript");
}

static esp_err_t get_estilo_css(httpd_req_t *req)
{
    return enviar_embebido(req, estilo_css_inicio, estilo_css_fin, "text/css");
}

static esp_err_t get_favicon(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

// WebSocket

static esp_err_t manejar_ws(httpd_req_t *req)
{
    /* Primera llamada: el handshake ya lo hizo el servidor */
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket conectado (fd %d)", httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    httpd_ws_frame_t marco = { 0 };

    /* Con largo 0 solo se lee el encabezado, para saber el tamaño */
    esp_err_t err = httpd_ws_recv_frame(req, &marco, 0);
    if (err != ESP_OK) {
        return err;
    }
    if (marco.len > LARGO_MAX_MENSAJE) {
        /* No se leyó el contenido: cerrar la conexión para no desincronizarla */
        ESP_LOGW(TAG, "mensaje de %u bytes, demasiado largo: se cierra la conexión",
                 (unsigned)marco.len);
        return ESP_FAIL;
    }

    uint8_t buf[LARGO_MAX_MENSAJE + 1];
    if (marco.len > 0) {
        marco.payload = buf;
        err = httpd_ws_recv_frame(req, &marco, marco.len);
        if (err != ESP_OK) {
            return err;
        }
    }
    buf[marco.len] = '\0';

    if (marco.type != HTTPD_WS_TYPE_TEXT || marco.len == 0) {
        return ESP_OK;
    }

    comando_t cmd;
    if (!protocolo_interpretar((const char *)buf, &cmd)) {
        ESP_LOGW(TAG, "mensaje desconocido: \"%s\"", (const char *)buf);
        return ESP_OK;
    }
    if (!estado_enviar_comando(&cmd)) {
        ESP_LOGW(TAG, "cola de comandos llena: se descartó \"%s\"", (const char *)buf);
    }
    return ESP_OK;
}

/* Se llama al cerrar cualquier conexión. Con close_fn propio hay que cerrar el socket. */
static void al_cerrar(httpd_handle_t hd, int fd)
{
    if (httpd_ws_get_fd_info(hd, fd) == HTTPD_WS_CLIENT_WEBSOCKET) {
        ESP_LOGI(TAG, "WebSocket cerrado (fd %d)", fd);
        comando_t cmd = { .tipo = CMD_CLIENTE_PERDIDO, .origen = ORIGEN_WEB };
        estado_enviar_comando(&cmd);
    }
    close(fd);
}

// Telemetría

/* Corre dentro de la tarea del servidor (httpd_queue_work) */
static void trabajo_telemetria(void *arg)
{
    static char json[LARGO_JSON];
    int    fds[MAX_SOCKETS];
    size_t n = MAX_SOCKETS;

    if (httpd_get_client_list(s_servidor, &n, fds) == ESP_OK) {
        int clientes = 0;
        for (size_t i = 0; i < n; i++) {
            if (httpd_ws_get_fd_info(s_servidor, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                clientes++;
            }
        }

        if (clientes > 0) {
            estado_sistema_t e;
            estado_obtener(&e);
            int largo = protocolo_estado_json(&e, clientes, json, sizeof(json));

            if (largo > 0) {
                httpd_ws_frame_t marco = {
                    .final   = true,
                    .type    = HTTPD_WS_TYPE_TEXT,
                    .payload = (uint8_t *)json,
                    .len     = (size_t)largo,
                };
                for (size_t i = 0; i < n; i++) {
                    if (httpd_ws_get_fd_info(s_servidor, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                        httpd_ws_send_frame_async(s_servidor, fds[i], &marco);
                    }
                }
            }
        }
    }

    atomic_store(&s_envio_pendiente, false);
}

static void tarea_telemetria(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_JUNIOR_TELEMETRIA_MS));

        /* Si el envío anterior no ha terminado, se salta este */
        if (atomic_exchange(&s_envio_pendiente, true)) {
            continue;
        }
        if (httpd_queue_work(s_servidor, trabajo_telemetria, NULL) != ESP_OK) {
            atomic_store(&s_envio_pendiente, false);
        }
    }
}

// Inicio

esp_err_t servidor_iniciar(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = MAX_SOCKETS;
    cfg.lru_purge_enable = true;      /* si se llena, cierra la conexión más vieja */
    cfg.close_fn         = al_cerrar;
    cfg.core_id          = 0;         /* mismo núcleo que el WiFi; control va en el 1 */

    ESP_RETURN_ON_ERROR(httpd_start(&s_servidor, &cfg), TAG,
                        "no se pudo iniciar el servidor HTTP");

    const httpd_uri_t rutas[] = {
        { .uri = "/",            .method = HTTP_GET, .handler = get_index      },
        { .uri = "/index.html",  .method = HTTP_GET, .handler = get_index      },
        { .uri = "/app.js",      .method = HTTP_GET, .handler = get_app_js     },
        { .uri = "/estilo.css",  .method = HTTP_GET, .handler = get_estilo_css },
        { .uri = "/favicon.ico", .method = HTTP_GET, .handler = get_favicon    },
        { .uri = "/ws",          .method = HTTP_GET, .handler = manejar_ws,
          .is_websocket = true },
    };
    for (size_t i = 0; i < sizeof(rutas) / sizeof(rutas[0]); i++) {
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_servidor, &rutas[i]), TAG,
                            "no se pudo registrar %s", rutas[i].uri);
    }

    BaseType_t ok = xTaskCreate(tarea_telemetria, "telemetria", TELEMETRIA_PILA_BYTES,
                                NULL, TELEMETRIA_PRIORIDAD, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "no se pudo crear la tarea de telemetría");

    ESP_LOGI(TAG, "servidor listo");
    return ESP_OK;
}
