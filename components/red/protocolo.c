/*
 * protocolo.c
 *
 * No usa ESP-IDF: se puede compilar y probar en la computadora
 * (ver test/test_protocolo.c).
 */

#include "protocolo.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char     *texto;
    comando_tipo_t  tipo;
    int             arg;
} entrada_t;

static const entrada_t TABLA[] = {
    { "MOV ADE",     CMD_MOVER,     MOV_ADELANTE  },
    { "MOV ATR",     CMD_MOVER,     MOV_ATRAS     },
    { "MOV IZQ",     CMD_MOVER,     MOV_IZQUIERDA },
    { "MOV DER",     CMD_MOVER,     MOV_DERECHA   },
    { "ALTO",        CMD_ALTO,      0             },
    { "PARO",        CMD_PARO,      0             },
    { "REANUDAR",    CMD_REANUDAR,  0             },
    { "VEL +",       CMD_VELOCIDAD, +1            },
    { "VEL -",       CMD_VELOCIDAD, -1            },
    { "MODO MANUAL", CMD_MODO,      MODO_MANUAL   },
    { "MODO LUZ",    CMD_MODO,      MODO_LUZ      },
    { "INFO",        CMD_INFO,      0             },
};

#define LARGO_MAX_MENSAJE  32

bool protocolo_interpretar(const char *texto, comando_t *cmd)
{
    if (texto == NULL || cmd == NULL) {
        return false;
    }

    /* Copia sin espacios ni saltos de línea al final */
    char limpio[LARGO_MAX_MENSAJE];
    size_t n = strnlen(texto, sizeof(limpio));
    if (n >= sizeof(limpio)) {
        return false;
    }
    memcpy(limpio, texto, n);
    while (n > 0 && (limpio[n - 1] == ' ' || limpio[n - 1] == '\r' ||
                     limpio[n - 1] == '\n' || limpio[n - 1] == '\t')) {
        n--;
    }
    limpio[n] = '\0';

    for (size_t i = 0; i < sizeof(TABLA) / sizeof(TABLA[0]); i++) {
        if (strcmp(limpio, TABLA[i].texto) != 0) {
            continue;
        }
        memset(cmd, 0, sizeof(*cmd));
        cmd->tipo   = TABLA[i].tipo;
        cmd->origen = ORIGEN_WEB;
        switch (cmd->tipo) {
        case CMD_MOVER:     cmd->arg.mov   = (movimiento_t)TABLA[i].arg; break;
        case CMD_VELOCIDAD: cmd->arg.pasos = (int8_t)TABLA[i].arg;       break;
        case CMD_MODO:      cmd->arg.modo  = (modo_t)TABLA[i].arg;       break;
        default: break;
        }
        return true;
    }
    return false;
}

static const char *codigo_mov(movimiento_t m)
{
    switch (m) {
    case MOV_ADELANTE:  return "ADE";
    case MOV_ATRAS:     return "ATR";
    case MOV_IZQUIERDA: return "IZQ";
    case MOV_DERECHA:   return "DER";
    default:            return "NIN";
    }
}

/*
 * Campos:
 *   modo      "manual" | "luz"
 *   hab       true si los puentes están habilitados (false = en paro)
 *   mov       "ADE" | "ATR" | "IZQ" | "DER" | "NIN"
 *   origen    "web" | "consola"
 *   vb, lim   velocidad base y límite, en %
 *   izq, der  salida de cada motor después de la rampa, en % con signo
 *   timeouts  altos por falta de comandos de la web
 *   perdidos  comandos descartados por cola llena
 *   clientes  WebSockets conectados
 *   ciclo     contador del lazo de control (si no avanza, el lazo se detuvo)
 */
int protocolo_estado_json(const estado_sistema_t *e, int clientes,
                          char *buf, size_t largo)
{
    if (e == NULL || buf == NULL || largo == 0) {
        return -1;
    }
    int n = snprintf(buf, largo,
        "{\"modo\":\"%s\",\"hab\":%s,\"mov\":\"%s\",\"origen\":\"%s\","
        "\"vb\":%.0f,\"lim\":%.0f,\"izq\":%.1f,\"der\":%.1f,"
        "\"timeouts\":%lu,\"perdidos\":%lu,\"clientes\":%d,\"ciclo\":%lu}",
        e->modo == MODO_LUZ ? "luz" : "manual",
        e->habilitado ? "true" : "false",
        codigo_mov(e->movimiento),
        e->origen_movimiento == ORIGEN_WEB ? "web" : "consola",
        (double)e->velocidad_base, (double)e->limite,
        (double)e->salida_izq, (double)e->salida_der,
        (unsigned long)e->timeouts, (unsigned long)e->comandos_perdidos,
        clientes, (unsigned long)e->ciclo);

    if (n < 0 || (size_t)n >= largo) {
        return -1;
    }
    return n;
}
