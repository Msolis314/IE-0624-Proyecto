/*
 * main.c (TEMPORAL) - Prueba del componente de motores sin osciloscopio
 *
 * Recorre varios niveles de velocidad, mantiene cada uno 5 s y muestra
 * en el log el duty y la frecuencia que reporta el LEDC, junto con el
 * voltaje promedio que debería leer un multímetro en cada pin.
 *
 * NO conectar los 12 V de los motores durante esta prueba.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "config_pines.h"
#include "motores.h"

static const char *TAG = "prueba";

#define VCC_LOGICA     3.3f
#define DUTY_MAX       (1U << PWM_RESOLUCION_BITS)
#define PERIODO_MS     20
#define SEGUNDOS_NIVEL 5

/* Niveles a probar en ambos motores (se recortan al límite configurado) */
static const float niveles[] = { 0.0f, 10.0f, 25.0f, 40.0f, -25.0f, 0.0f };

/* Ejecuta el lazo de control durante 'segundos', igual que la tarea real */
static void correr_lazo(int segundos)
{
    TickType_t ultimo = xTaskGetTickCount();
    for (int i = 0; i < segundos * 1000 / PERIODO_MS; i++) {
        motores_actualizar(PERIODO_MS / 1000.0f);
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_MS));
    }
}

static void reportar(float objetivo)
{
    uint32_t freq   = ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);
    uint32_t d_rpwm = ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    uint32_t d_lpwm = ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    ESP_LOGI(TAG, "objetivo %+6.1f %% | salida izq %+6.1f %% der %+6.1f %%",
             objetivo,
             motores_obtener_salida(MOTOR_IZQ),
             motores_obtener_salida(MOTOR_DER));
    ESP_LOGI(TAG, "  LEDC: %lu Hz | izq RPWM duty %4lu/%u -> %.2f V | izq LPWM duty %4lu/%u -> %.2f V",
             (unsigned long)freq,
             (unsigned long)d_rpwm, DUTY_MAX, VCC_LOGICA * d_rpwm / DUTY_MAX,
             (unsigned long)d_lpwm, DUTY_MAX, VCC_LOGICA * d_lpwm / DUTY_MAX);
}

void app_main(void)
{
    ESP_ERROR_CHECK(motores_init());
    ESP_LOGI(TAG, "EN debe medir 0 V. Se habilita en 3 s...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    motores_habilitar(true);
    ESP_LOGI(TAG, "EN debe medir 3.3 V");

    for (int n = 0; n < sizeof(niveles) / sizeof(niveles[0]); n++) {
        motores_set_objetivo(niveles[n], niveles[n]);
        correr_lazo(1);              /* dejar que la rampa llegue */
        reportar(niveles[n]);
        correr_lazo(SEGUNDOS_NIVEL); /* tiempo para medir */
    }

    /* Prueba del paro: subir y cortar de golpe */
    motores_set_objetivo(40.0f, 40.0f);
    correr_lazo(1);
    ESP_LOGI(TAG, "antes del paro:");
    reportar(40.0f);
    motores_paro();
    ESP_LOGI(TAG, "después del paro (todo debe estar en 0 y EN en 0 V):");
    reportar(0.0f);

    ESP_LOGI(TAG, "prueba terminada");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}