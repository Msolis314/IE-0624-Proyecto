#ifndef ESTADO_H
#define ESTADO_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/*
 * Estado compartido y cola de comandos.
 *
 * Es el único punto de contacto entre las partes del sistema:
 *
 *   consola ─┐
 *            ├─> comando_t ──> [cola] ──> control ──> motores
 *   red ─────┘                              │
 *                                            └──> estado_sistema_t ──> red (telemetría)
 *
 * Reglas:
 *   - Cualquiera puede ENVIAR comandos. Solo la tarea de control los RECIBE.
 *   - Solo la tarea de control PUBLICA el estado. Los demás solo lo LEEN (copia).
 *   - Este archivo no conoce motores, WiFi ni la página web. Si cambia la
 *     implementación de cualquiera de ellos, este contrato no cambia.
 */

/* ------------------------------------------------------------------------ */
/* Tipos                                                                     */
/* ------------------------------------------------------------------------ */

typedef enum {
    MODO_MANUAL = 0,
    MODO_LUZ,              /* fase 5; por ahora control lo rechaza */
} modo_t;

typedef enum {
    ORIGEN_CONSOLA = 0,    /* teclado del monitor serial (pruebas) */
    ORIGEN_WEB,            /* aplicación web por WebSocket */
} origen_t;

typedef enum {
    MOV_NINGUNO = 0,
    MOV_ADELANTE,
    MOV_ATRAS,
    MOV_IZQUIERDA,         /* giro en el sitio */
    MOV_DERECHA,
} movimiento_t;

typedef enum {
    CMD_MOVER = 0,         /* arg.mov. Desde la web se repite mientras el botón está presionado */
    CMD_ALTO,              /* alto suave, con rampa; también sale del modo luz */
    CMD_PARO,              /* paro de seguridad: inmediato, deshabilita los puentes */
    CMD_REANUDAR,          /* habilita los puentes */
    CMD_VELOCIDAD,         /* arg.pasos: +1 sube un paso, -1 baja un paso */
    CMD_MODO,              /* arg.modo */
    CMD_CLIENTE_PERDIDO,   /* la red avisa que se cerró una conexión */
    CMD_INFO,              /* imprimir el estado en el log */
} comando_tipo_t;

typedef struct {
    comando_tipo_t tipo;
    origen_t       origen;
    union {
        movimiento_t mov;
        int8_t       pasos;
        modo_t       modo;
    } arg;
} comando_t;

typedef struct {
    modo_t       modo;
    bool         habilitado;          /* puentes H habilitados (false = en paro) */
    movimiento_t movimiento;          /* lo que se pidió por última vez */
    origen_t     origen_movimiento;   /* quién lo pidió */
    float        velocidad_base;      /* % */
    float        limite;              /* % (MOTOR_LIMITE_PORCENTAJE) */
    float        salida_izq;          /* % con signo, después de la rampa */
    float        salida_der;
    uint32_t     timeouts;            /* altos por falta de comandos de la web */
    uint32_t     comandos_perdidos;   /* comandos descartados por cola llena */
    uint32_t     ciclo;               /* contador de ciclos del lazo de control */
} estado_sistema_t;

/* ------------------------------------------------------------------------ */
/* Funciones                                                                 */
/* ------------------------------------------------------------------------ */

/* Crea la cola y el mutex. Se llama una vez, antes que todo lo demás. */
esp_err_t estado_iniciar(void);

/*
 * Envía un comando a la tarea de control. No bloquea.
 * Devuelve false si la cola está llena y el comando se descartó.
 * CMD_PARO nunca se pierde: si la cola está llena, igual se ejecuta.
 */
bool estado_enviar_comando(const comando_t *cmd);

/* Solo para la tarea de control: saca el siguiente comando, sin bloquear. */
bool estado_recibir_comando(comando_t *cmd);

/* Solo para la tarea de control: publica una copia del estado actual. */
void estado_publicar(const estado_sistema_t *estado);

/* Para cualquiera: obtiene una copia del último estado publicado. */
void estado_obtener(estado_sistema_t *estado);

/* Comandos descartados por cola llena desde el arranque. */
uint32_t estado_comandos_perdidos(void);

#endif /* ESTADO_H */
