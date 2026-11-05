#ifndef PROTOCOLO_H
#define PROTOCOLO_H

/*
 * Protocolo entre la aplicación web y la ESP32 (privado del componente red).
 *
 * Navegador -> ESP32: texto corto, un comando por mensaje
 *   MOV ADE | MOV ATR | MOV IZQ | MOV DER   (repetir cada 100 ms mientras se presiona)
 *   ALTO | PARO | REANUDAR
 *   VEL + | VEL -
 *   MODO MANUAL | MODO LUZ
 *   INFO
 *
 * ESP32 -> navegador: un objeto JSON con el estado (ver protocolo.c).
 *
 * Toda la traducción está aquí: si cambia el formato, no cambia nada más.
 */

#include <stdbool.h>
#include <stddef.h>
#include "estado.h"

/* Convierte un mensaje de texto en comando. Devuelve false si no se reconoce. */
bool protocolo_interpretar(const char *texto, comando_t *cmd);

/* Escribe el estado como JSON. Devuelve el largo escrito, o -1 si no cupo. */
int protocolo_estado_json(const estado_sistema_t *e, int clientes,
                          char *buf, size_t largo);

#endif /* PROTOCOLO_H */
