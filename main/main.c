#include "esp_log.h"
#include "config_pines.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Plataforma Junior: inicio");
    ESP_LOGI(TAG, "Motor izq: RPWM=%d LPWM=%d EN=%d",
             PIN_MOTOR_IZQ_RPWM, PIN_MOTOR_IZQ_LPWM, PIN_MOTOR_IZQ_EN);
    ESP_LOGI(TAG, "Motor der: RPWM=%d LPWM=%d EN=%d",
             PIN_MOTOR_DER_RPWM, PIN_MOTOR_DER_LPWM, PIN_MOTOR_DER_EN);
}
