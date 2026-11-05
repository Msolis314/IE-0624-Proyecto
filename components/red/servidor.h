#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "esp_err.h"

/* Servidor HTTP (páginas embebidas + WebSocket) y tarea de telemetría.
 * Privado del componente red. */
esp_err_t servidor_iniciar(void);

#endif /* SERVIDOR_H */
