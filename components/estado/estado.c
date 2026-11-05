/*
 * estado.c
 *
 * Cola de comandos (muchos productores, un consumidor: la tarea de control)
 * y copia del estado protegida con un mutex (un escritor, muchos lectores).
 */

#include "estado.h"

#include <stdatomic.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "estado";

#define LARGO_COLA_COMANDOS  16

static QueueHandle_t     s_cola  = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static estado_sistema_t  s_estado;

/*
 * Respaldo para el paro: si la cola está llena cuando llega CMD_PARO, se
 * marca esta bandera. La tarea de control la revisa antes que la cola y,
 * si está activa, descarta todo lo que estaba en cola (era anterior al paro).
 */
static atomic_bool     s_paro_pendiente = false;
static atomic_int      s_paro_origen    = ORIGEN_CONSOLA;
static atomic_uint_fast32_t s_perdidos  = 0;

esp_err_t estado_iniciar(void)
{
    ESP_RETURN_ON_FALSE(s_cola == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "el estado ya estaba iniciado");

    s_cola  = xQueueCreate(LARGO_COLA_COMANDOS, sizeof(comando_t));
    s_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_cola != NULL && s_mutex != NULL, ESP_ERR_NO_MEM, TAG,
                        "no se pudo crear la cola o el mutex");

    memset(&s_estado, 0, sizeof(s_estado));
    return ESP_OK;
}

bool estado_enviar_comando(const comando_t *cmd)
{
    if (s_cola == NULL || cmd == NULL) {
        return false;
    }

    if (xQueueSend(s_cola, cmd, 0) == pdTRUE) {
        return true;
    }

    if (cmd->tipo == CMD_PARO) {
        atomic_store(&s_paro_origen, (int)cmd->origen);
        atomic_store(&s_paro_pendiente, true);
        return true;
    }

    atomic_fetch_add(&s_perdidos, 1);
    return false;
}

bool estado_recibir_comando(comando_t *cmd)
{
    if (s_cola == NULL || cmd == NULL) {
        return false;
    }

    if (atomic_exchange(&s_paro_pendiente, false)) {
        xQueueReset(s_cola);
        cmd->tipo   = CMD_PARO;
        cmd->origen = (origen_t)atomic_load(&s_paro_origen);
        ESP_LOGW(TAG, "cola llena al llegar un paro: se descartaron los comandos previos");
        return true;
    }

    return xQueueReceive(s_cola, cmd, 0) == pdTRUE;
}

void estado_publicar(const estado_sistema_t *estado)
{
    if (s_mutex == NULL || estado == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado = *estado;
    s_estado.comandos_perdidos = (uint32_t)atomic_load(&s_perdidos);
    xSemaphoreGive(s_mutex);
}

void estado_obtener(estado_sistema_t *estado)
{
    if (s_mutex == NULL || estado == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *estado = s_estado;
    xSemaphoreGive(s_mutex);
}

uint32_t estado_comandos_perdidos(void)
{
    return (uint32_t)atomic_load(&s_perdidos);
}
