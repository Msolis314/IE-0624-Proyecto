/*
 * control.c - control manual por WiFi y por consola
 *
 * Este archivo contiene la lógica de control de los motores,
 * ejecutada en una tarea periódica de 20 ms.
 */


 #include "control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "config_pines.h"
#include "estado.h"
#include "motores.h"

static const char *TAG = "control";

// Parametros de control

#define CONTROL_PERIODO_MS     20
#define CONTROL_PILA_BYTES     4096
#define CONTROL_PRIORIDAD      10
#define CONTROL_NUCLEO         1      /* el WiFi corre en el núcleo 0 */

#define VELOCIDAD_INICIAL      30.0f  /* % */
#define PASO_VELOCIDAD         10.0f  /* % por cada +/- */

/* Sin comandos de la web por este tiempo, en modo manual se hace un alto */
#define TIMEOUT_WEB_MS         500


// Estado interno

typedef struct {
    estado_sistema_t e;
    int64_t ultimo_mov_us;
} control_t;

static const char *nombre_mov(movimiento_t m)
{
    switch (m) {
    case MOV_ADELANTE:  return "adelante";
    case MOV_ATRAS:     return "atrás";
    case MOV_IZQUIERDA: return "izquierda";
    case MOV_DERECHA:   return "derecha";
    default:            return "alto";
    }
}


static const char *nombre_origen(origen_t o)
{
    return (o == ORIGEN_WEB) ? "web" : "consola";
}

/* Traduce el movimiento pedido a objetivos de los motores */
static void aplicar_movimiento(const control_t *c)
{
    const float v = c->e.velocidad_base;
    float izq = 0.0f;
    float der = 0.0f;

    switch (c->e.movimiento) {
    case MOV_ADELANTE:  izq =  v; der =  v; break;
    case MOV_ATRAS:     izq = -v; der = -v; break;
    case MOV_IZQUIERDA: izq = -v; der =  v; break;
    case MOV_DERECHA:   izq =  v; der = -v; break;
    default: break;
    }
    motores_set_objetivo(izq, der);
}

static void detener_suave(control_t *c)
{
    c->e.movimiento = MOV_NINGUNO;
    aplicar_movimiento(c);
}

static void imprimir_estado(const control_t *c)
{
    ESP_LOGI(TAG, "modo %s | puentes %s | %s (%s) | salida izq %+6.1f %% der %+6.1f %% | "
                  "base %.0f %% (límite %.0f %%) | timeouts %lu | perdidos %lu",
             c->e.modo == MODO_MANUAL ? "manual" : "luz",
             c->e.habilitado ? "habilitados" : "deshabilitados",
             nombre_mov(c->e.movimiento), nombre_origen(c->e.origen_movimiento),
             motores_obtener_salida(MOTOR_IZQ), motores_obtener_salida(MOTOR_DER),
             c->e.velocidad_base, c->e.limite,
             (unsigned long)c->e.timeouts,
             (unsigned long)estado_comandos_perdidos());
}


// Procesamiento de comandos

static void procesar(control_t *c, const comando_t *cmd, int64_t ahora_us)
{
    switch (cmd->tipo) {

    case CMD_MOVER:
        if (!c->e.habilitado) {
            /* Desde la web llega cada 100 ms: solo se avisa al cambiar */
            if (cmd->origen == ORIGEN_CONSOLA) {
                ESP_LOGW(TAG, "en paro: presione 'r' para reanudar");
            }
            break;
        }
        if (c->e.modo != MODO_MANUAL) {
            break;
        }
        if (cmd->arg.mov == MOV_NINGUNO) {
            detener_suave(c);
            break;
        }
        c->ultimo_mov_us = ahora_us;
        if (cmd->arg.mov != c->e.movimiento || cmd->origen != c->e.origen_movimiento) {
            c->e.movimiento        = cmd->arg.mov;
            c->e.origen_movimiento = cmd->origen;
            aplicar_movimiento(c);
            ESP_LOGI(TAG, "%s (%.0f %%, %s)", nombre_mov(c->e.movimiento),
                     c->e.velocidad_base, nombre_origen(cmd->origen));
        }
        break;

    case CMD_ALTO:
        if (c->e.modo != MODO_MANUAL) {
            c->e.modo = MODO_MANUAL;
            ESP_LOGI(TAG, "modo manual");
        }
        if (c->e.movimiento != MOV_NINGUNO) {
            ESP_LOGI(TAG, "alto (%s)", nombre_origen(cmd->origen));
        }
        detener_suave(c);
        break;

    case CMD_PARO:
        motores_paro();
        c->e.movimiento = MOV_NINGUNO;
        c->e.modo       = MODO_MANUAL;    /* tras un paro nunca se vuelve solo al modo luz */
        ESP_LOGW(TAG, "PARO (%s)", nombre_origen(cmd->origen));
        break;

    case CMD_REANUDAR:
        if (!motores_habilitados()) {
            c->e.movimiento = MOV_NINGUNO;
            motores_habilitar(true);
            ESP_LOGI(TAG, "puentes habilitados (%s)", nombre_origen(cmd->origen));
        }
        break;

    case CMD_VELOCIDAD: {
        float v = c->e.velocidad_base + cmd->arg.pasos * PASO_VELOCIDAD;
        if (v < 0.0f)       v = 0.0f;
        if (v > c->e.limite) v = c->e.limite;
        c->e.velocidad_base = v;
        aplicar_movimiento(c);
        ESP_LOGI(TAG, "velocidad base %.0f %% (límite %.0f %%)", v, c->e.limite);
        break;
    }

    case CMD_MODO:
        if (cmd->arg.modo == MODO_MANUAL) {
            c->e.modo = MODO_MANUAL;
            detener_suave(c);
            ESP_LOGI(TAG, "modo manual");
        } else {
            ESP_LOGW(TAG, "modo luz todavía no implementado; se queda en manual");
        }
        break;

    case CMD_CLIENTE_PERDIDO:
        /* Manual: si la web estaba moviendo la plataforma, se detiene ya.
         * Luz: se ignora, la búsqueda continúa. */
        if (c->e.modo == MODO_MANUAL &&
            c->e.movimiento != MOV_NINGUNO &&
            c->e.origen_movimiento == ORIGEN_WEB) {
            ESP_LOGW(TAG, "se cerró una conexión web: alto");
            detener_suave(c);
        }
        break;

    case CMD_INFO:
        imprimir_estado(c);
        break;

    default:
        break;
    }
}

/*
 * Timeout de la web. Solo aplica a movimientos pedidos desde la web: la
 * aplicación repite el comando cada 100 ms mientras el botón está
 * presionado. Los de la consola no vencen (pruebas de banco de la fase 1).
 */
static void revisar_timeout(control_t *c, int64_t ahora_us)
{
    if (c->e.modo != MODO_MANUAL ||
        c->e.movimiento == MOV_NINGUNO ||
        c->e.origen_movimiento != ORIGEN_WEB) {
        return;
    }
    if (ahora_us - c->ultimo_mov_us > (int64_t)TIMEOUT_WEB_MS * 1000) {
        c->e.timeouts++;
        ESP_LOGW(TAG, "%d ms sin comandos de la web: alto", TIMEOUT_WEB_MS);
        detener_suave(c);
    }
}

// Tarea

static void tarea_control(void *arg)
{
    control_t c = {
        .e = {
            .modo              = MODO_MANUAL,
            .habilitado        = false,
            .movimiento        = MOV_NINGUNO,
            .origen_movimiento = ORIGEN_CONSOLA,
            .limite            = MOTOR_LIMITE_PORCENTAJE,
            .velocidad_base    = (VELOCIDAD_INICIAL < MOTOR_LIMITE_PORCENTAJE)
                                 ? VELOCIDAD_INICIAL : MOTOR_LIMITE_PORCENTAJE,
        },
        .ultimo_mov_us = 0,
    };
    const float dt_s = CONTROL_PERIODO_MS / 1000.0f;
    TickType_t ultimo = xTaskGetTickCount();
    comando_t cmd;

    while (1) {
        const int64_t ahora_us = esp_timer_get_time();

        /* 1. Comandos pendientes */
        while (estado_recibir_comando(&cmd)) {
            procesar(&c, &cmd, ahora_us);
        }

        /* 2. Timeout de la web */
        revisar_timeout(&c, ahora_us);

        /* 3. Rampa y PWM */
        motores_actualizar(dt_s);

        /* 4. Publicar el estado */
        c.e.habilitado = motores_habilitados();
        c.e.salida_izq = motores_obtener_salida(MOTOR_IZQ);
        c.e.salida_der = motores_obtener_salida(MOTOR_DER);
        c.e.ciclo++;
        estado_publicar(&c.e);

        /* 5. Esperar el siguiente periodo */
        xTaskDelayUntil(&ultimo, pdMS_TO_TICKS(CONTROL_PERIODO_MS));
    }
}

esp_err_t control_iniciar(void)
{
    BaseType_t ok = xTaskCreatePinnedToCore(tarea_control, "control",
                                            CONTROL_PILA_BYTES, NULL,
                                            CONTROL_PRIORIDAD, NULL,
                                            CONTROL_NUCLEO);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "no se pudo crear la tarea de control");
    return ESP_OK;
}
