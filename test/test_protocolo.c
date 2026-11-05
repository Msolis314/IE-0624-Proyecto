/*
 * Prueba del protocolo en la computadora (no necesita la ESP32):
 *
 *   gcc -Wall -Wextra -I components/estado/include -I components/red \
 *       -I test/stubs test/test_protocolo.c components/red/protocolo.c \
 *       -o build_test && ./build_test
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "protocolo.h"

static int fallas = 0;

#define VERIFICAR(cond) do { \
    if (!(cond)) { printf("FALLA línea %d: %s\n", __LINE__, #cond); fallas++; } \
} while (0)

int main(void)
{
    comando_t c;

    VERIFICAR(protocolo_interpretar("MOV ADE", &c));
    VERIFICAR(c.tipo == CMD_MOVER && c.arg.mov == MOV_ADELANTE && c.origen == ORIGEN_WEB);
    VERIFICAR(protocolo_interpretar("MOV ATR", &c) && c.arg.mov == MOV_ATRAS);
    VERIFICAR(protocolo_interpretar("MOV IZQ", &c) && c.arg.mov == MOV_IZQUIERDA);
    VERIFICAR(protocolo_interpretar("MOV DER", &c) && c.arg.mov == MOV_DERECHA);
    VERIFICAR(protocolo_interpretar("ALTO", &c) && c.tipo == CMD_ALTO);
    VERIFICAR(protocolo_interpretar("PARO", &c) && c.tipo == CMD_PARO);
    VERIFICAR(protocolo_interpretar("PARO\r\n", &c) && c.tipo == CMD_PARO);
    VERIFICAR(protocolo_interpretar("REANUDAR", &c) && c.tipo == CMD_REANUDAR);
    VERIFICAR(protocolo_interpretar("VEL +", &c) && c.tipo == CMD_VELOCIDAD && c.arg.pasos == 1);
    VERIFICAR(protocolo_interpretar("VEL -", &c) && c.arg.pasos == -1);
    VERIFICAR(protocolo_interpretar("MODO LUZ", &c) && c.tipo == CMD_MODO && c.arg.modo == MODO_LUZ);
    VERIFICAR(protocolo_interpretar("MODO MANUAL", &c) && c.arg.modo == MODO_MANUAL);
    VERIFICAR(protocolo_interpretar("INFO", &c) && c.tipo == CMD_INFO);

    /* Mensajes inválidos (prueba W11) */
    VERIFICAR(!protocolo_interpretar("", &c));
    VERIFICAR(!protocolo_interpretar("mov ade", &c));
    VERIFICAR(!protocolo_interpretar("MOV", &c));
    VERIFICAR(!protocolo_interpretar("MOV ADELANTE", &c));
    VERIFICAR(!protocolo_interpretar("{\"cmd\":\"paro\"}", &c));
    VERIFICAR(!protocolo_interpretar("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", &c));
    VERIFICAR(!protocolo_interpretar(NULL, &c));

    /* JSON */
    estado_sistema_t e = {
        .modo = MODO_MANUAL, .habilitado = true, .movimiento = MOV_IZQUIERDA,
        .origen_movimiento = ORIGEN_WEB, .velocidad_base = 30, .limite = 40,
        .salida_izq = -30, .salida_der = 30, .timeouts = 2, .comandos_perdidos = 0,
        .ciclo = 123456,
    };
    char buf[320];
    int n = protocolo_estado_json(&e, 1, buf, sizeof(buf));
    printf("%s\n(%d bytes)\n", buf, n);
    VERIFICAR(n > 0);
    VERIFICAR(strstr(buf, "\"mov\":\"IZQ\"") != NULL);
    VERIFICAR(strstr(buf, "\"hab\":true") != NULL);
    VERIFICAR(strstr(buf, "\"izq\":-30.0") != NULL);

    /* Peor caso de largo */
    e.timeouts = 4294967295u; e.comandos_perdidos = 4294967295u; e.ciclo = 4294967295u;
    e.salida_izq = -100; e.salida_der = -100; e.velocidad_base = 100; e.limite = 100;
    n = protocolo_estado_json(&e, 99, buf, sizeof(buf));
    printf("peor caso: %d bytes\n", n);
    VERIFICAR(n > 0);
    VERIFICAR(protocolo_estado_json(&e, 1, buf, 20) == -1);

    printf(fallas ? "\n%d FALLAS\n" : "\nTodo bien\n", fallas);
    return fallas ? 1 : 0;
}
