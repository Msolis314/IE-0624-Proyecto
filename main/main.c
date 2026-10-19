/*
 * main.c - Fase 1: control de motores desde la consola serial
 *
 * Tarea de control provisional de 20 ms que es la única que escribe en
 * los motores. La consola solo pone teclas en una cola; la tarea de
 * control las procesa al inicio de cada ciclo (como en la figura A.4).
 *
 * En la fase 3 la cola de teclas se reemplaza por la cola de comando_t
 * que llena el servidor WebSocket, y se agrega el paro por 500 ms sin
 * comandos.
 *
 * Teclas:
 *   w  adelante        s  atrás
 *   a  girar izquierda d  girar derecha
 *   x  alto suave (con rampa)
 *   p  paro de seguridad (inmediato, deshabilita puentes)
 *   r  reanudar (habilita puentes)
 *   +  subir velocidad base 10 %   -  bajar velocidad base 10 %
 *   i  mostrar estado              ?  ayuda
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "config_pines.h"
#include "motores.h"
#include "consola.h"

static const char *TAG = "main";

#define PERIODO_CONTROL_MS   20
#define CONTROL_PILA_BYTES   4096
#define CONTROL_PRIORIDAD    10
#define LARGO_COLA_TECLAS    16

#define VELOCIDAD_INICIAL    30.0f
#define PASO_VELOCIDAD       10.0f

static QueueHandle_t s_cola_teclas;

/* ------------------------------------------------------------------------ */
/* Consola: solo pone la tecla en la cola                                    */
/* ------------------------------------------------------------------------ */

static void al_recibir_tecla(char tecla)
{
    /* Si la cola está llena, la tecla se descarta */
    xQueueSend(s_cola_teclas, &tecla, 0);
}

static void mostrar_ayuda(void)
{
    ESP_LOGI(TAG, "w/s adelante/atrás, a/d girar, x alto, p paro, r reanudar, "
                  "+/- velocidad, i estado, ? ayuda");
}

/* ------------------------------------------------------------------------ */
/* Tarea de control                                                          */
/* ------------------------------------------------------------------------ */

typedef struct {
    int   dir_izq;     /* -1, 0 o 1 */
    int   dir_der;
    float velocidad;   /* velocidad base en % */
} control_t;

static void aplicar_objetivo(const control_t *c)
{
    motores_set_objetivo(c->dir_izq * c->velocidad, c->dir_der * c->velocidad);
}

static void fijar_direccion(control_t *c, int izq, int der, const char *nombre)
{
    if (!motores_habilitados()) {
        ESP_LOGW(TAG, "en paro: presione 'r' para reanudar");
        return;
    }
    c->dir_izq = izq;
    c->dir_der = der;
    aplicar_objetivo(c);
    ESP_LOGI(TAG, "%s (%.0f %%)", nombre, c->velocidad);
}

static void procesar_tecla(control_t *c, char tecla)
{
    switch (tecla) {
    case 'w': fijar_direccion(c,  1,  1, "adelante");  break;
    case 's': fijar_direccion(c, -1, -1, "atrás");     break;
    case 'a': fijar_direccion(c, -1,  1, "izquierda"); break;
    case 'd': fijar_direccion(c,  1, -1, "derecha");   break;
    case 'x': fijar_direccion(c,  0,  0, "alto");      break;

    case 'p':
        c->dir_izq = 0;
        c->dir_der = 0;
        motores_paro();
        break;

    case 'r':
        if (!motores_habilitados()) {
            c->dir_izq = 0;
            c->dir_der = 0;
            motores_habilitar(true);
        }
        break;

    case '+':
    case '-':
        c->velocidad += (tecla == '+') ? PASO_VELOCIDAD : -PASO_VELOCIDAD;
        if (c->velocidad < 0.0f)   c->velocidad = 0.0f;
        if (c->velocidad > 100.0f) c->velocidad = 100.0f;
        aplicar_objetivo(c);
        ESP_LOGI(TAG, "velocidad base %.0f %% (límite %.0f %%)",
                 c->velocidad, MOTOR_LIMITE_PORCENTAJE);
        break;

    case 'i':
        ESP_LOGI(TAG, "puentes %s | salida izq %+6.1f %% der %+6.1f %% | base %.0f %%",
                 motores_habilitados() ? "habilitados" : "deshabilitados",
                 motores_obtener_salida(MOTOR_IZQ),
                 motores_obtener_salida(MOTOR_DER),
                 c->velocidad);
        break;

    case '?':
        mostrar_ayuda();
        break;

    default:
        break;
    }
}

static void tarea_control(void *arg)
{
    control_t control = {
        .dir_izq   = 0,
        .dir_der   = 0,
        .velocidad = VELOCIDAD_INICIAL,
    };
    const float dt_s = PERIODO_CONTROL_MS / 1000.0f;
    TickType_t ultimo = xTaskGetTickCount();
    char tecla;

    while (1) {
        /* 1. Procesar todos los comandos pendientes */
        while (xQueueReceive(s_cola_teclas, &tecla, 0) == pdTRUE) {
            procesar_tecla(&control, tecla);
        }

        /* 2. Rampa y escritura del PWM */
        motores_actualizar(dt_s);

        /* 3. Esperar el siguiente periodo */
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_CONTROL_MS));
    }
}

/* ------------------------------------------------------------------------ */
/* Inicio                                                                    */
/* ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "Plataforma Junior: fase 1, control por consola");

    ESP_ERROR_CHECK(motores_init());

    s_cola_teclas = xQueueCreate(LARGO_COLA_TECLAS, sizeof(char));
    if (s_cola_teclas == NULL) {
        ESP_LOGE(TAG, "no se pudo crear la cola de teclas");
        return;
    }

    xTaskCreate(tarea_control, "control", CONTROL_PILA_BYTES,
                NULL, CONTROL_PRIORIDAD, NULL);

    ESP_ERROR_CHECK(consola_iniciar(al_recibir_tecla));

    /* Arranca deshabilitado: hay que presionar 'r' para mover los motores */
    ESP_LOGI(TAG, "motores deshabilitados; presione 'r' para habilitar");
    mostrar_ayuda();
}