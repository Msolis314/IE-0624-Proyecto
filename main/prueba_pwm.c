/*
 * prueba_pwm.c - Prueba del PWM de los motores con osciloscopio
 *
 * Se compila EN LUGAR de main.c con:   make prueba-pwm
 * (usa la carpeta build_pwm/ y sube el límite de velocidad a 100 %).
 *
 *
 * Teclas:
 *   0..9  nivel 0 % a 90 %        m  nivel 100 %
 *   n     invertir sentido        c  alternar sentido cada 1.5 s (on/off)
 *   t     marcador por ciclo de control (on/off)
 *   p     paro de seguridad       r  rehabilitar puentes
 *   i     reportar duty actual    ?  ayuda
 */

#include <math.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "config_pines.h"
#include "motores.h"
#include "consola.h"

static const char *TAG = "prueba_pwm";

/* GPIO 7 está libre en la asignación del proyecto; solo se usa aquí */
#define PIN_MARCADOR          GPIO_NUM_7

#define PERIODO_CONTROL_MS    20
#define PERIODO_ALTERNAR_MS   1500
#define CONTROL_PILA_BYTES    4096
#define CONTROL_PRIORIDAD     10

#define DUTY_MAX              (1U << PWM_RESOLUCION_BITS)
#define PERIODO_PWM_US        (1000000.0f / PWM_FRECUENCIA_HZ)

static QueueHandle_t s_cola_teclas;

typedef struct {
    float nivel;            /* magnitud en % */
    int   signo;            /* 1 adelante, -1 atrás */
    bool  alternar;         /* cambiar de sentido periódicamente */
    int   ms_alternar;      /* tiempo desde el último cambio */
    bool  marcador_ciclo;   /* true: el marcador cambia en cada ciclo */
    bool  marcador_alto;
    bool  reporte_pendiente;
} prueba_t;

/* ------------------------------------------------------------------------ */
/* Auxiliares                                                                */
/* ------------------------------------------------------------------------ */

static void al_recibir_tecla(char tecla)
{
    xQueueSend(s_cola_teclas, &tecla, 0);
}

static void mostrar_ayuda(void)
{
    ESP_LOGI(TAG, "0..9 nivel 0-90 %%, m 100 %%, n invertir, c alternar, "
                  "t marcador por ciclo, p paro, r rehabilitar, i reporte, ? ayuda");
}

static void marcador(prueba_t *p, bool nivel)
{
    p->marcador_alto = nivel;
    gpio_set_level(PIN_MARCADOR, nivel ? 1 : 0);
}

/* Velocidad que debería quedar después de la rampa (ya recortada) */
static float objetivo_esperado(const prueba_t *p)
{
    float v = p->signo * p->nivel;
    if (v >  MOTOR_LIMITE_PORCENTAJE) v =  MOTOR_LIMITE_PORCENTAJE;
    if (v < -MOTOR_LIMITE_PORCENTAJE) v = -MOTOR_LIMITE_PORCENTAJE;
    return v;
}

static void aplicar(prueba_t *p)
{
    float v = p->signo * p->nivel;
    motores_set_objetivo(v, v);
    p->reporte_pendiente = true;
}

/* Muestra lo que el LEDC tiene en cada canal y lo que se debe medir */
static void reportar(void)
{
    /* Mismo orden de canales que en motores.c */
    static const struct {
        const char    *nombre;
        ledc_channel_t canal;
        int            gpio;
    } canales[] = {
        { "izq RPWM", LEDC_CHANNEL_0, PIN_MOTOR_IZQ_RPWM },
        { "izq LPWM", LEDC_CHANNEL_1, PIN_MOTOR_IZQ_LPWM },
        { "der RPWM", LEDC_CHANNEL_2, PIN_MOTOR_DER_RPWM },
        { "der LPWM", LEDC_CHANNEL_3, PIN_MOTOR_DER_LPWM },
    };

    uint32_t freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);

    ESP_LOGI(TAG, "salida izq %+6.1f %% der %+6.1f %% | LEDC %lu Hz, periodo %.1f us",
             motores_obtener_salida(MOTOR_IZQ),
             motores_obtener_salida(MOTOR_DER),
             (unsigned long)freq, PERIODO_PWM_US);

    for (int i = 0; i < sizeof(canales) / sizeof(canales[0]); i++) {
        uint32_t d = ledc_get_duty(LEDC_LOW_SPEED_MODE, canales[i].canal);
        ESP_LOGI(TAG, "  GPIO %2d %s: duty %4lu/%u = %5.1f %% -> tiempo en alto %5.2f us",
                 canales[i].gpio, canales[i].nombre,
                 (unsigned long)d, DUTY_MAX,
                 100.0f * d / DUTY_MAX,
                 PERIODO_PWM_US * d / DUTY_MAX);
    }
}

/* ------------------------------------------------------------------------ */
/* Procesamiento de teclas                                                   */
/* ------------------------------------------------------------------------ */

static void procesar_tecla(prueba_t *p, char tecla)
{
    /* Flanco de subida del marcador justo antes de actuar: sirve de
     * disparo para ver el inicio de la rampa, el paro o el cambio de sentido. */
    if (!p->marcador_ciclo) {
        marcador(p, true);
    }

    if (tecla >= '0' && tecla <= '9') {
        p->nivel = (tecla - '0') * 10.0f;
    } else if (tecla == 'm') {
        p->nivel = 100.0f;
    }

    switch (tecla) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'm':
        if (!motores_habilitados()) {
            ESP_LOGW(TAG, "puentes deshabilitados: presione 'r'");
            break;
        }
        aplicar(p);
        ESP_LOGI(TAG, "nivel %+.0f %%", p->signo * p->nivel);
        break;

    case 'n':
        p->signo = -p->signo;
        aplicar(p);
        ESP_LOGI(TAG, "sentido %s", p->signo > 0 ? "adelante (RPWM)" : "atrás (LPWM)");
        break;

    case 'c':
        p->alternar = !p->alternar;
        p->ms_alternar = 0;
        ESP_LOGI(TAG, "alternar sentido cada %d ms: %s",
                 PERIODO_ALTERNAR_MS, p->alternar ? "sí" : "no");
        break;

    case 't':
        p->marcador_ciclo = !p->marcador_ciclo;
        marcador(p, false);
        ESP_LOGI(TAG, "marcador (GPIO %d): %s", PIN_MARCADOR,
                 p->marcador_ciclo ? "cambia en cada ciclo de control"
                                   : "pulso en cada comando");
        break;

    case 'p':
        p->alternar = false;
        motores_paro();
        reportar();
        break;

    case 'r':
        if (!motores_habilitados()) {
            p->nivel = 0.0f;
            p->signo = 1;
            motores_habilitar(true);
        }
        break;

    case 'i':
        reportar();
        break;

    case '?':
        mostrar_ayuda();
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------------ */
/* Tarea de control (misma estructura que en main.c)                         */
/* ------------------------------------------------------------------------ */

static void tarea_control(void *arg)
{
    prueba_t p = {
        .nivel = 0.0f,
        .signo = 1,
    };
    const float dt_s = PERIODO_CONTROL_MS / 1000.0f;
    TickType_t ultimo = xTaskGetTickCount();
    char tecla;

    while (1) {
        /* El pulso del marcador de comando dura un ciclo (20 ms) */
        if (!p.marcador_ciclo && p.marcador_alto) {
            marcador(&p, false);
        }

        /* 1. Comandos pendientes */
        while (xQueueReceive(s_cola_teclas, &tecla, 0) == pdTRUE) {
            procesar_tecla(&p, tecla);
        }

        /* 2. Cambio de sentido automático */
        if (p.alternar && motores_habilitados()) {
            p.ms_alternar += PERIODO_CONTROL_MS;
            if (p.ms_alternar >= PERIODO_ALTERNAR_MS) {
                p.ms_alternar = 0;
                p.signo = -p.signo;
                if (!p.marcador_ciclo) {
                    marcador(&p, true);
                }
                aplicar(&p);
            }
        }

        /* 3. Rampa y PWM */
        motores_actualizar(dt_s);

        /* 4. Marcador por ciclo: onda cuadrada con medio periodo de 20 ms */
        if (p.marcador_ciclo) {
            marcador(&p, !p.marcador_alto);
        }

        /* 5. Reportar una vez que la rampa llegó al objetivo */
        if (p.reporte_pendiente && motores_habilitados() &&
            fabsf(motores_obtener_salida(MOTOR_IZQ) - objetivo_esperado(&p)) < 0.01f) {
            p.reporte_pendiente = false;
            if (!p.alternar) {
                reportar();
            }
        }

        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_CONTROL_MS));
    }
}


/* Inicio                                                                    */
void app_main(void)
{
    ESP_LOGI(TAG, "PRUEBA DE PWM CON OSCILOSCOPIO (no conectar 12 V)");
    ESP_LOGI(TAG, "límite de velocidad: %.0f %% (debe decir 100 en esta prueba)",
             MOTOR_LIMITE_PORCENTAJE);

    ESP_ERROR_CHECK(motores_init());

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_MARCADOR,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(PIN_MARCADOR, 0);

    s_cola_teclas = xQueueCreate(16, sizeof(char));
    if (s_cola_teclas == NULL) {
        ESP_LOGE(TAG, "no se pudo crear la cola");
        return;
    }

    xTaskCreate(tarea_control, "control", CONTROL_PILA_BYTES,
                NULL, CONTROL_PRIORIDAD, NULL);
    ESP_ERROR_CHECK(consola_iniciar(al_recibir_tecla));

    ESP_LOGI(TAG, "puentes deshabilitados; presione 'r' para empezar");
    mostrar_ayuda();
}