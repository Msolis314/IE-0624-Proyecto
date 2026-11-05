#ifndef RED_H
#define RED_H

#include "esp_err.h"

/*
 * Red: punto de acceso WiFi, servidor HTTP con la aplicación web y
 * WebSocket en /ws.
 *
 * - Lo que llega por el WebSocket se traduce a comando_t y se envía con
 *   estado_enviar_comando(). La red nunca llama a funciones de motores.
 * - Cada CONFIG_JUNIOR_TELEMETRIA_MS envía el estado (estado_obtener())
 *   a todos los clientes como JSON.
 * - Al cerrarse un WebSocket envía CMD_CLIENTE_PERDIDO.
 *
 * SSID, contraseña, canal y tasa de telemetría: make menuconfig,
 * menú "Junior: red WiFi".
 *
 * Antes de llamarla: nvs_flash_init() y estado_iniciar().
 */
esp_err_t red_iniciar(void);

#endif /* RED_H */
