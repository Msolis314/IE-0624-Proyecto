# Plataforma Junior: movilidad con ESP32-S3

Proyecto del curso IE0624 Laboratorio de Microcontroladores (II-2026), Universidad de Costa Rica.

Una ESP32-S3 controla los dos motores DC de la plataforma de la microrred fotovoltaica Junior, usando dos puentes H BTS7960. La plataforma se maneja por WiFi desde una aplicación web que sirve la propia ESP32. En una fase posterior, un sensor TSL2591 le permitirá buscar el punto con más luz y estacionarse ahí.

| Fase | Contenido | Estado |
|---|---|---|
| 1 | PWM de 20 kHz, rampa, paro y consola de pruebas | Completa |
| 2 | Punto de acceso WiFi, WebSocket y aplicación web | Completa |
| 4 | Sensores I2C: TSL2591 y MPU-6050 | Pendiente |
| 5 | Modo luz: buscar y estacionarse | Pendiente |

---

## 1. Conexiones

### 1.1 Pines de la ESP32-S3

| Señal | GPIO | Va a |
|---|---|---|
| Motor izquierdo RPWM | 4 | RPWM del BTS7960 izquierdo |
| Motor izquierdo LPWM | 5 | LPWM del BTS7960 izquierdo |
| Motor izquierdo EN | 6 | R_EN y L_EN del BTS7960 izquierdo, unidos |
| Motor derecho RPWM | 15 | RPWM del BTS7960 derecho |
| Motor derecho LPWM | 16 | LPWM del BTS7960 derecho |
| Motor derecho EN | 17 | R_EN y L_EN del BTS7960 derecho, unidos |
| I2C SDA (fase 4) | 8 | SDA del TSL2591 y del MPU-6050 |
| I2C SCL (fase 4) | 9 | SCL del TSL2591 y del MPU-6050 |
| Marcador del osciloscopio | 7 | Punta del osciloscopio (solo en `make prueba-pwm`) |
| GND | GND | GND de ambos BTS7960, de los sensores y negativo de los 12 V |

No usar estos pines para otra cosa:

| GPIO | Motivo |
|---|---|
| 0, 3, 45, 46 | Pines de *strapping*: definen el modo de arranque |
| 19, 20 | USB nativo (programación y consola en `/dev/ttyACM0`) |
| 26 a 37 | Flash y PSRAM octal |
| 43, 44 | UART0 |

### 1.2 Cada módulo BTS7960 (IBT-2)

| Pin del módulo | Conexión |
|---|---|
| RPWM | GPIO de RPWM del motor correspondiente |
| LPWM | GPIO de LPWM del motor correspondiente |
| R_EN y L_EN | Unidos entre sí, al GPIO de EN, con una **resistencia de 10 kΩ de EN a GND** |
| R_IS, L_IS | Sin conectar (no se usa la medición de corriente) |
| VCC | Alimentación de la lógica del módulo (ver la advertencia de abajo) |
| GND | GND común con la ESP32 |
| B+ / B− | +12 V / negativo de la fuente de los motores |
| M+ / M− | Terminales del motor |

Las resistencias pull-down de 10 kΩ en EN son obligatorias. 

**Nivel lógico de 3.3 V (prueba E1):** el buffer de entrada del IBT-2 (74HC244) puede no reconocer los 3.3 V de la ESP32 como un alto si VCC del módulo está a 5 V. Si la prueba E1 falla, hay que alimentar VCC del módulo con 3.3 V o agregar un convertidor de nivel.

Si un motor gira al revés, no hace falta cambiar los cables:  poner su `MOTOR_*_INVERTIDO` en `1`, en `components/config/include/config_pines.h`.

### 1.3 Sensores I2C (fase 4)

| Sensor | Dirección | VCC | SDA / SCL |
|---|---|---|---|
| TSL2591 (luz) | 0x29 | 3.3 V | GPIO 8 / GPIO 9 |
| MPU-6050 (IMU) | 0x68 (AD0 a GND) | 3.3 V | GPIO 8 / GPIO 9 |

### 1.4 Alimentación

```
Fuente 12 V ──┬── B+ BTS7960 izquierdo
              └── B+ BTS7960 derecho

USB de la PC ──── ESP32-S3 (puerto USB nativo)

GND común: ESP32, ambos BTS7960, sensores y negativo de los 12 V
```

---

## 2. Entorno de desarrollo

- ESP-IDF v6.1, instalado con el ESP-IDF Installation Manager (EIM):

  ```bash
  sudo apt install eim
  eim install -i v6.1
  ```

- El `Makefile` activa el entorno solo, con `~/.espressif/tools/activate_idf_v6.1.sh`.
- El usuario debe estar en el grupo `dialout` para acceder al puerto serial: `sudo usermod -aG dialout $USER`, y luego cerrar y abrir la sesión.
- La placa aparece como `/dev/ttyACM0`.

### Primera compilación (o después de cambiar `sdkconfig.defaults`)

```bash
rm -f sdkconfig sdkconfig.old
make fullclean
make set-target
make fm
```

`sdkconfig.defaults` solo se aplica cuando no existe `sdkconfig`. Si se cambia y no se borra `sdkconfig`, la compilación sigue usando la configuración vieja.

### Comandos del Makefile

| Comando | Acción |
|---|---|
| `make` | Compila |
| `make flash` | Compila y flashea |
| `make monitor` | Abre el monitor serial |
| `make fm` | Flashea y abre el monitor |
| `make prueba-pwm` | Compila y flashea la prueba del osciloscopio (en `build_pwm/`) |
| `make set-target` | Fija el chip `esp32s3` |
| `make menuconfig` | Configuración (incluye el menú "Junior: red WiFi") |
| `make size` | Tamaño del firmware |
| `make clean` / `make fullclean` | Borra la compilación |
| `make erase` | Borra toda la flash |
| `make help` | Lista los comandos |

Se puede cambiar el puerto o la velocidad: `make PORT=/dev/ttyUSB0 BAUD=460800 fm`.

Para salir del monitor: `Ctrl+]`, o `Ctrl+T` y luego `Ctrl+X`.

---

## 3. Pruebas

 La evidencia de cada prueba se guarda en `docs/pruebas/AAAA-MM-DD_<ID>`.

### 3.1 Prueba del PWM con osciloscopio (`make prueba-pwm`)

```bash
make prueba-pwm
```

Este programa reemplaza a `main.c`: se compila en `build_pwm/` y sube el límite de velocidad a 100 %. Al arrancar, el log debe indicar un límite de 100 %. **No conecte los 12 V de los motores:** la medición se hace directamente en los GPIO.

| Tecla | Acción |
|---|---|
| `0` a `9` | Nivel de 0 % a 90 % |
| `m` | Nivel de 100 % |
| `n` | Invertir el sentido |
| `c` | Alternar el sentido cada 1.5 s (encender o apagar) |
| `t` | Marcador en GPIO 7 en cada ciclo de control (onda de 40 ms) |
| `p` | Paro de seguridad |
| `r` | Rehabilitar los puentes |
| `i` | Reportar el duty actual |
| `?` | Ayuda |

El marcador en GPIO 7 sube justo antes de cada comando. Hay que dispararle el osciloscopio para medir latencias.

| Medición | Cómo | Valor esperado |
|---|---|---|
| Frecuencia (M3) | Cualquier nivel | 20 kHz (periodo de 50 µs) |
| Ciclo de trabajo (M4) | Teclas `1`, `2`, `5`, `7` | 5, 10, 25 y 35 µs en alto |
| Extremos (M5) | Teclas `0` y `m` | Bajo constante / alto constante |
| Exclusión RPWM/LPWM (M6) | Tecla `c`, dos canales | Nunca están los dos en alto a la vez |
| Rampa (M7) | Filtro RC de 10 kΩ y 100 nF, 100 ms/div, de `0` a `m` | 500 ms |
| Latencia del paro (S3) | Tecla `p`, disparo en el marcador | EN y PWM en 0 en ≤ 21 ms |
| Periodo del lazo (M10) | Tecla `t`, persistencia | Onda cuadrada de 40 ms |
| Arranque sin pulsos (S1) | Disparo único en EN y PWM, presionar RESET | Ningún pulso |

### 3.2 Prueba de banco con los motores a 12 V (`make fm`)

Antes de empezar: ruedas en el aire, mano cerca del corte de los 12 V y `MOTOR_LIMITE_PORCENTAJE` en 40.

```bash
make fm
```

Las teclas se escriben directamente en el monitor, sin Enter:

| Tecla | Acción |
|---|---|
| `w` / `s` | Adelante / atrás |
| `a` / `d` | Girar en el sitio a la izquierda / derecha |
| `x` | Alto suave, con rampa |
| `p` | Paro de seguridad: inmediato, deshabilita los puentes |
| `r` | Reanudar (habilitar los puentes) |
| `+` / `-` | Velocidad base ±10 % (arranca en 30 %) |
| `i` | Estado actual |
| `?` | Ayuda |

El sistema arranca con los puentes deshabilitados, así que hay que presionar `r` antes de moverlo. Los movimientos que se ordenan desde la consola no tienen timeout.

Qué revisar:

1. Con `r` y luego `w`, ambas ruedas giran hacia adelante (M11).
2. Con `a` y `d`, la plataforma gira hacia el lado correcto (M12).
3. Al cambiar varias veces entre `w` y `s`, la transición es suave y no hay `rst:` ni `Brownout` en el log (M13).
4. Con `w` y luego `p`, se detiene de inmediato, y `w` no hace nada hasta presionar `r` (S3, S4).
5. Con `+` varias veces, nunca supera el 40 % (M9).

### 3.3 Prueba de la aplicación web (`make fm`)

1. Flashear con `make fm`. En el log debe aparecer `red "Junior-ESP32" lista`.
2. Conectar el teléfono o la laptop a la red **Junior-ESP32**, con la contraseña **junior2026**.
3. Abrir `http://192.168.4.1`. El indicador de la esquina debe decir "Conectado".
4. Presionar **Reanudar** y mantener presionado un botón de dirección.

Desde una laptop también se puede usar el teclado: `W A S D` o las flechas para moverse, `X` para el alto, `P` o la barra espaciadora para el paro, `R` para reanudar y `+` / `−` para la velocidad.

| Prueba | Cómo | Resultado esperado |
|---|---|---|
| Botones (W5) | Mantener presionado y soltar cada botón | Se mueve mientras se presiona y se detiene al soltar |
| Timeout de 500 ms (W7) | Mientras avanza: cerrar la pestaña o apagar el WiFi del teléfono | Se detiene; en "Diagnóstico" aumenta "Timeouts" |
| Sin tartamudeo (W8) | Mantener un botón 30 s | Salida constante |
| Mensajes inválidos (W11) | Enviar texto desconocido por el WebSocket | Se ignora; en el log aparece `mensaje desconocido` |
| Consola junto con la web (W14) | Presionar `p` e `i` en el monitor mientras se maneja desde la web | La consola responde y el paro funciona |

Para cambiar el nombre de la red, la contraseña, el canal o el periodo de la telemetría: `make menuconfig` → "Junior: red WiFi".

### 3.4 Prueba del protocolo en la computadora

Esta prueba no necesita la ESP32. Verifica que cada mensaje de la web se traduzca al comando correcto y que el JSON de la telemetría se genere bien.

```bash
gcc -Wall -Wextra -I components/estado/include -I components/red \
    -I test/stubs test/test_protocolo.c components/red/protocolo.c \
    -o build_test && ./build_test
```

Al final debe imprimir `Todo bien`.

---

## 4. Estructura del repositorio

```
IE-0624-Proyecto/
├── CMakeLists.txt            proyecto; con PRUEBA_PWM sube el límite a 100 %
├── Makefile                  envoltorio de idf.py (ver la sección 2)
├── partitions.csv            tabla de particiones: app de 4 MB
├── sdkconfig.defaults        flash 16 MB, PSRAM octal, FreeRTOS 1 kHz,
│                             consola USB, particiones propias, WebSocket
├── LICENSE
├── main/
│   ├── CMakeLists.txt        elige main.c o prueba_pwm.c
│   ├── main.c                arranque y traducción de teclas a comandos
│   └── prueba_pwm.c          prueba del PWM con osciloscopio
├── components/
│   ├── config/               config_pines.h: pines y constantes
│   ├── motores/              PWM de 20 kHz, rampa y paro (BTS7960)
│   ├── consola/              lectura de teclas por el USB nativo
│   ├── estado/               cola de comandos y copia del estado
│   ├── control/              tarea de 20 ms, la única que escribe en los motores
│   └── red/
│       ├── red.c             punto de acceso WiFi
│       ├── servidor.c        HTTP, WebSocket y telemetría
│       ├── protocolo.c       traducción de mensajes y JSON
│       └── Kconfig           menú "Junior: red WiFi"
├── web/                      aplicación web, embebida en el firmware
│   ├── index.html
│   ├── app.js
│   └── estilo.css
├── test/
│   ├── test_protocolo.c      prueba del protocolo en la computadora
│   └── stubs/                sustitutos de encabezados de ESP-IDF
└── docs/
    └── pruebas/              evidencia de las pruebas, con fecha
```

### Cómo se comunican los módulos

```
consola ─┐
         ├─> comando_t ─> [cola] ─> control ─> motores
red ─────┘                            │
                                      └─> estado_sistema_t ─> red (telemetría)
```

- La tarea de control es la única que llama a funciones de `motores.h`.
- `consola` y `red` solo envían comandos (`comando_t`) y leen copias del estado. No conocen los motores.
- Para cambiar el PWM, la rampa o los pines basta con editar `motores` o `config`. Para cambiar el formato de los mensajes, `protocolo.c` y `app.js`. En ninguno de los dos casos hay que tocar el resto.

### Parámetros que se suelen ajustar

| Parámetro | Archivo | Valor actual |
|---|---|---|
| `MOTOR_LIMITE_PORCENTAJE` | `components/config/include/config_pines.h` | 40 % (subir después de las pruebas de banco) |
| `MOTOR_IZQ_INVERTIDO`, `MOTOR_DER_INVERTIDO` | `components/config/include/config_pines.h` | 0 |
| `RAMPA_PORCENTAJE_POR_S` | `components/config/include/config_pines.h` | 200 (de 0 a 100 % en 0.5 s) |
| `TIMEOUT_WEB_MS` | `components/control/control.c` | 500 ms |
| `VELOCIDAD_INICIAL`, `PASO_VELOCIDAD` | `components/control/control.c` | 30 %, 10 % |
| SSID, contraseña, canal, telemetría | `make menuconfig` → "Junior: red WiFi" | Junior-ESP32, junior2026, 6, 200 ms |

---

## Licencia

MIT. Ver `LICENSE`.