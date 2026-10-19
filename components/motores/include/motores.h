#ifndef MOTORES_H
#define MOTORES_H

#include <stdbool.h>
#include "esp_err.h"

/*
Control de 2 motores DC mediante puentes H BTS7960

Convencion de velocidad -> valores en porcentaje de -100 a 100
> 0 avance (RPWM activo, LPWM en 0)
< 0 retroceso (LPWM activo, RPWM en 0)
= 0 detenido
*/

// Identificador de cada motor

typedef enum {
    MOTOR_IZQ = 0,
    MOTOR_DER,
    MOTOR_CANTIDAD
} motor_id_t;


// Configura los GPIO y el PWM (LEDC)
esp_err_t motores_init(void);

// Habilita o deshabilita ambos puentes H mediantes los  pines EN.
void motores_habilitar(bool habilitar);


// Fija la velocidad de cada motor.
void motores_set_objetivo(float izq, float der);

// Avanza la rampa un paso dt_s segundos y escribe el PWM
// La tarea de control la llama en cada ciclo (cada 20 ms)
void motores_actualizar(float dt_s);

// Paro de seguridad: pone objetivo y salida en 0
void motores_paro(void);

// Devuelve la velocidad actual aplicada actualmente
float motores_obtener_salida(motor_id_t motor);

// Indica si los puentes H están habilitados
bool motores_habilitados(void);

#endif // MOTORES_H