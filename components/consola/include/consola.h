#ifndef CONSOLA_H
#define CONSOLA_H

#include "esp_err.h"

/*
 * Consola de pruebas por el USB nativo del ESP32-S3 (/dev/ttyACM0).
 *
 * Crea una tarea que lee las teclas que se escriben en el monitor serial
 * (make monitor) y llama a una función por cada tecla recibida.
 *
 * La consola no conoce los motores ni los comandos: solo entrega teclas.
 * Quien la usa decide qué hacer con cada una. La función de retorno corre
 * en la tarea de la consola, así que debe ser corta (por ejemplo, poner
 * un comando en una cola) y nunca escribir directamente en los motores.
 */

 // Funcion que se llama con cada tecla recibida
 typedef void (*consola_cb_t)(char tecla);

 // Instala el driver USB serial/JTAG y crea la tarea de lectura
 esp_err_t consola_iniciar(consola_cb_t al_recibir);

 #endif /* CONSOLA_H */