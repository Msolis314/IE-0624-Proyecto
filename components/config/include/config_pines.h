#ifndef CONFIG_PINES_H
#define CONFIG_PINES_H

#include "driver/gpio.h"

/*  Motor izquierdo (BTS7960) */
#define PIN_MOTOR_IZQ_RPWM   GPIO_NUM_4
#define PIN_MOTOR_IZQ_LPWM   GPIO_NUM_5
#define PIN_MOTOR_IZQ_EN     GPIO_NUM_6

/*Motor derecho (BTS7960) */
#define PIN_MOTOR_DER_RPWM   GPIO_NUM_15
#define PIN_MOTOR_DER_LPWM   GPIO_NUM_16
#define PIN_MOTOR_DER_EN     GPIO_NUM_17

/* Bus I2C (sensores TSL2591 y MPU-6050)*/
#define PIN_I2C_SDA          GPIO_NUM_8
#define PIN_I2C_SCL          GPIO_NUM_9

/*Parámetros del PWM*/
#define PWM_FRECUENCIA_HZ    20000   /* 20 kHz, fuera del rango audible */
#define PWM_RESOLUCION_BITS  11      /* máximo posible a 20 kHz con reloj de 80 MHz */

/*Rampa de aceleración */
#define RAMPA_PORCENTAJE_POR_S  200.0f  /* de 0 a 100 % en 0.5 s */

/* Inversión de sentido por software*/
#define MOTOR_IZQ_INVERTIDO  0
#define MOTOR_DER_INVERTIDO  0

#endif /* CONFIG_PINES_H */
