#ifndef CONTROL_H
#define CONTROL_H

#include "esp_err.h"

/* Tarea de control: la UNICA que escribe en los motores
*
* Cada 20 ms:
* 1. Saca todos los comandos pendientes de la cola (estado.h);
* 2. revisa el timeout de 500 ms de la web en modo manual;
* 3. llama a motores_actualizar()  (rampa y PWM);
* 4. Publica una copia del estado para la telemetria
* Solo depende de estado.h y motores.h
*/

esp_err_t control_iniciar(void);

#endif /* CONTROL_H */