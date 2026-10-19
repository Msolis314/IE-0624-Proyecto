/*
 * motores.c
 * Funciones para controlar los motores
 * PWM de 20 kHz, rampa de aceleracion y paro de seguridad
 * 
 */

#include "motores.h"

#include <math.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"

#include "config_pines.h"

static const char *TAG = "motores";

#define PWM_TIMER LEDC_TIMER_0
#define PWM_MODO LEDC_LOW_SPEED_MODE
#define PWM_DUTY_MAX (1U << PWM_RESOLUCION_BITS) // 2048 = 100 %

// Estado de un motor y su puente H
typedef struct {
    float salida; // velocidad aplicada, de -100 a 100
    float objetivo; // velocidad deseada antes de la rampa
    gpio_num_t pin_rpwm, pin_lpwm, pin_en;
    ledc_channel_t canal_rpwm, canal_lpwm;
    bool invertido; // si es true, se invierte el sentido de giro
} motor_t;


static motor_t s_motores[MOTOR_CANTIDAD] = {
    [MOTOR_IZQ] = {
        .pin_rpwm = PIN_MOTOR_IZQ_RPWM,
        .pin_lpwm = PIN_MOTOR_IZQ_LPWM,
        .pin_en = PIN_MOTOR_IZQ_EN,
        .canal_rpwm = LEDC_CHANNEL_0,
        .canal_lpwm = LEDC_CHANNEL_1,
        .invertido = MOTOR_IZQ_INVERTIDO,
    },
    [MOTOR_DER] = {
        .pin_rpwm = PIN_MOTOR_DER_RPWM,
        .pin_lpwm = PIN_MOTOR_DER_LPWM,
        .pin_en = PIN_MOTOR_DER_EN,
        .canal_rpwm = LEDC_CHANNEL_2,
        .canal_lpwm = LEDC_CHANNEL_3,
        .invertido = MOTOR_DER_INVERTIDO,
    },
};

static bool s_inicializado = false;
static bool s_habilitados = false;

/*Funciones auxiliares*/

static float recortar(float valor, float minimo, float maximo) {
    if (valor < minimo) {
        return minimo;
    } else if (valor > maximo) {
        return maximo;
    } else {
        return valor;
    }
}

static void escribir_duty(ledc_channel_t canal, uint32_t duty) {
    ledc_set_duty(PWM_MODO, canal, duty);
    ledc_update_duty(PWM_MODO, canal);
}

static void escribir_motor(const motor_t *m) {
    float v = m->invertido ? -m->salida : m->salida;
    uint32_t duty = (uint32_t)lroundf(fabsf(v) / 100.0f * (float)PWM_DUTY_MAX);
    if (duty > PWM_DUTY_MAX) {
        duty = PWM_DUTY_MAX;
    }

    /*Primero se apaga el lado inactivo y luego se enciende el activo
    *para que nunca esten ambos en alto al mismo tiempo*/

    if (v > 0.0f) {
        escribir_duty(m->canal_lpwm, 0);
        escribir_duty(m->canal_rpwm, duty);
    } else if (v < 0.0f) {
        escribir_duty(m->canal_rpwm, 0);
        escribir_duty(m->canal_lpwm, duty);
    } else {
        escribir_duty(m->canal_rpwm, 0);
        escribir_duty(m->canal_lpwm, 0);
    }

}

static void escribir_en(bool nivel){
    for (int i = 0; i < MOTOR_CANTIDAD; i++) {
        gpio_set_level(s_motores[i].pin_en, nivel ? 1 : 0);
    }
}

static esp_err_t configurar_canal(gpio_num_t pin, ledc_channel_t canal){
    ledc_channel_config_t config = {
        .gpio_num = pin,
        .speed_mode = PWM_MODO,
        .channel = canal,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    return ledc_channel_config(&config);
}


/*Interfaz publica*/

esp_err_t motores_init(void) {
    if (s_inicializado) {
        return ESP_ERR_INVALID_STATE;
    }

    // Pines EN como salida y en bajo, antes de configurar PWM
    gpio_config_t cfg_en = {
        .pin_bit_mask = (1ULL << PIN_MOTOR_IZQ_EN) | (1ULL << PIN_MOTOR_DER_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&cfg_en), TAG, "Error al configurar pines EN");
    escribir_en(false);

    // Temporizador LEDC compartido por los 4 canales
    ledc_timer_config_t config_timer = {
        .speed_mode = PWM_MODO,
        .duty_resolution = (ledc_timer_bit_t)PWM_RESOLUCION_BITS,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FRECUENCIA_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&config_timer), TAG, "Error al configurar temporizador PWM");

    // 4 canales PWM, todos iniciados en 0
    for (int i = 0; i < MOTOR_CANTIDAD; i++) {
        motor_t *m = &s_motores[i];
        ESP_RETURN_ON_ERROR(configurar_canal(m->pin_rpwm, m->canal_rpwm), TAG, "no se pudo configurar RPWM del moter %d", i);
        ESP_RETURN_ON_ERROR(configurar_canal(m->pin_lpwm, m->canal_lpwm), TAG, "no se pudo configurar LPWM del motor %d", i);

        m->salida = 0.0f;
        m->objetivo = 0.0f;
    }
    s_inicializado = true;
    s_habilitados = false;
    ESP_LOGI(TAG, "PWM a %d Hz con %d bits; motores dehabilitados", PWM_FRECUENCIA_HZ, PWM_RESOLUCION_BITS);
    return ESP_OK;
}

void motores_habilitar(bool habilitar)
{
    if (!s_inicializado) {
        return;
    }
 
    if (!habilitar) {
        /* Al deshabilitar se reinicia todo en 0, para que al volver a
         * habilitar la rampa arranque desde cero y no haya saltos. */
        for (int i = 0; i < MOTOR_CANTIDAD; i++) {
            s_motores[i].objetivo = 0.0f;
            s_motores[i].salida   = 0.0f;
            escribir_motor(&s_motores[i]);
        }
    }
 
    escribir_en(habilitar);
    s_habilitados = habilitar;
    ESP_LOGI(TAG, "puentes H %s", habilitar ? "habilitados" : "deshabilitados");
}
 
void motores_set_objetivo(float izq, float der)
{
    s_motores[MOTOR_IZQ].objetivo =
        recortar(izq, -MOTOR_LIMITE_PORCENTAJE, MOTOR_LIMITE_PORCENTAJE);
    s_motores[MOTOR_DER].objetivo =
        recortar(der, -MOTOR_LIMITE_PORCENTAJE, MOTOR_LIMITE_PORCENTAJE);
}
 
void motores_actualizar(float dt_s)
{
    if (!s_inicializado || !s_habilitados || dt_s <= 0.0f) {
        return;
    }
 
    const float paso_max = RAMPA_PORCENTAJE_POR_S * dt_s;
 
    for (int i = 0; i < MOTOR_CANTIDAD; i++) {
        motor_t *m = &s_motores[i];
        float diferencia = m->objetivo - m->salida;
 
        if (fabsf(diferencia) <= paso_max) {
            m->salida = m->objetivo;
        } else {
            m->salida += copysignf(paso_max, diferencia);
        }
 
        escribir_motor(m);
    }
}
 
void motores_paro(void)
{
    if (!s_inicializado) {
        return;
    }
 
    /* Sin rampa: todo a cero de inmediato y puentes deshabilitados. */
    for (int i = 0; i < MOTOR_CANTIDAD; i++) {
        s_motores[i].objetivo = 0.0f;
        s_motores[i].salida   = 0.0f;
        escribir_motor(&s_motores[i]);
    }
    escribir_en(false);
    s_habilitados = false;
 
    ESP_LOGW(TAG, "paro de seguridad");
}
 
float motores_obtener_salida(motor_id_t motor)
{
    if (motor < 0 || motor >= MOTOR_CANTIDAD) {
        return 0.0f;
    }
    return s_motores[motor].salida;
}
 
bool motores_habilitados(void)
{
    return s_habilitados;
}