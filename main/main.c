/*
 * main.c - control manual por WiFi y por consola
 *
 * Este archivo solo arranca los módulos y traduce teclas de la consola
 * a comandos. La lógica está en los componentes:
 *
 *   motores/  PWM, rampa y paro (fase 1)
 *   estado/   cola de comando_t y copia del estado (contrato entre módulos)
 *   control/  tarea de 20 ms, única que escribe en los motores
 *   red/      punto de acceso, página web, WebSocket y telemetría
 *   consola/  teclas por el USB nativo (pruebas)
 *
 * Teclas de la consola (iguales a la fase 1):
 *   w  adelante        s  atrás
 *   a  girar izquierda d  girar derecha
 *   x  alto suave (con rampa)
 *   p  paro de seguridad (inmediato, deshabilita puentes)
 *   r  reanudar (habilita puentes)
 *   +  subir velocidad base 10 %   -  bajar velocidad base 10 %
 *   i  mostrar estado              ?  ayuda
 *
 * Los movimientos de la consola no vencen (no hay repetición de teclas);
 * los de la web se detienen si pasan 500 ms sin repetirse.
 */

#include "esp_log.h"
#include "nvs_flash.h"

#include "consola.h"
#include "control.h"
#include "estado.h"
#include "motores.h"
#include "red.h"

static const char *TAG = "main";

static void mostrar_ayuda(void)
{
    ESP_LOGI(TAG, "w/s adelante/atrás, a/d girar, x alto, p paro, r reanudar, "
                  "+/- velocidad, i estado, ? ayuda");
}

/* Corre en la tarea de la consola: solo traduce y encola */
static void al_recibir_tecla(char tecla)
{
    comando_t cmd = { .origen = ORIGEN_CONSOLA };

    switch (tecla) {
    case 'w': cmd.tipo = CMD_MOVER; cmd.arg.mov = MOV_ADELANTE;  break;
    case 's': cmd.tipo = CMD_MOVER; cmd.arg.mov = MOV_ATRAS;     break;
    case 'a': cmd.tipo = CMD_MOVER; cmd.arg.mov = MOV_IZQUIERDA; break;
    case 'd': cmd.tipo = CMD_MOVER; cmd.arg.mov = MOV_DERECHA;   break;
    case 'x': cmd.tipo = CMD_ALTO;                               break;
    case 'p': cmd.tipo = CMD_PARO;                               break;
    case 'r': cmd.tipo = CMD_REANUDAR;                           break;
    case '+': cmd.tipo = CMD_VELOCIDAD; cmd.arg.pasos = +1;      break;
    case '-': cmd.tipo = CMD_VELOCIDAD; cmd.arg.pasos = -1;      break;
    case 'i': cmd.tipo = CMD_INFO;                               break;
    case '?': mostrar_ayuda(); return;
    default:  return;
    }

    if (!estado_enviar_comando(&cmd)) {
        ESP_LOGW(TAG, "cola llena: se descartó '%c'", tecla);
    }
}

static void iniciar_nvs(void)
{
    /* El WiFi guarda su calibración en NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Plataforma Junior: fase 2, control por WiFi y consola");

    /* 1. Motores primero: EN queda en bajo lo antes posible */
    ESP_ERROR_CHECK(motores_init());

    /* 2. Contrato entre módulos y lazo de control */
    ESP_ERROR_CHECK(estado_iniciar());
    ESP_ERROR_CHECK(control_iniciar());

    /* 3. Entradas de comandos */
    ESP_ERROR_CHECK(consola_iniciar(al_recibir_tecla));

    iniciar_nvs();
    esp_err_t err = red_iniciar();
    if (err != ESP_OK) {
        /* Sin red la consola sigue funcionando: no se reinicia */
        ESP_LOGE(TAG, "la red no arrancó (%s); solo queda la consola",
                 esp_err_to_name(err));
    }

    /* Arranca deshabilitado: hay que presionar 'r' o Reanudar */
    ESP_LOGI(TAG, "motores deshabilitados; presione 'r' para habilitar");
    mostrar_ayuda();
}
