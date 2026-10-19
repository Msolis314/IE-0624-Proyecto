/*
 * consola.c
 *
 * Lectura de teclas por el USB nativo (USB Serial/JTAG) del ESP32-S3.
 * Se usa con el monitor de ESP-IDF (make monitor): cada tecla que se
 * presiona en la terminal llega aquí sin necesidad de presionar Enter.
 */

 #include "consola.h"

 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "driver/usb_serial_jtag.h"
 #include "driver/usb_serial_jtag_vfs.h"
 #include "esp_check.h"
 #include "esp_log.h"

 static const char *TAG = "consola";

 #define CONSOLA_PILA_BYTES 3072
 #define CONSOLA_PRIORIDAD 3 // menor que la tarea de control

 #define CONSOLA_BUFFER 16

 static consola_cb_t s_al_recibir = NULL;

 static void tarea_consola(void *arg) {
    uint8_t buffer[CONSOLA_BUFFER];

    while (1) {
        // Bloquea hasta que llegue un byte
        int n = usb_serial_jtag_read_bytes(buffer, sizeof(buffer), portMAX_DELAY);

        for (int i = 0; i < n ; i++) {
            char c = (char)buffer[i];
            if (c == '\r' || c == '\n') {
                continue; // ignorar retorno de carro y salto de línea
            }
            s_al_recibir(c);
        }
    }
 }

 esp_err_t consola_iniciar(consola_cb_t al_recibir)
{
    ESP_RETURN_ON_FALSE(al_recibir != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "falta la función de retorno");
    ESP_RETURN_ON_FALSE(s_al_recibir == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "la consola ya estaba iniciada");
 
    /* Driver con buffers por interrupción para poder leer sin perder bytes */
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(usb_serial_jtag_driver_install(&cfg), TAG,
                        "no se pudo instalar el driver USB Serial/JTAG");
 
    /* Los printf y ESP_LOG pasan también por el driver, así lectura y
     * escritura no compiten por el mismo hardware. */
    usb_serial_jtag_vfs_use_driver();
 
    s_al_recibir = al_recibir;
 
    BaseType_t ok = xTaskCreate(tarea_consola, "consola", CONSOLA_PILA_BYTES,
                                NULL, CONSOLA_PRIORIDAD, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "no se pudo crear la tarea de la consola");
 
    ESP_LOGI(TAG, "consola lista");
    return ESP_OK;
}