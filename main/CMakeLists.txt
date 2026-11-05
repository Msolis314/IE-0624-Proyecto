# El programa normal es main.c. Con "make prueba-pwm" (que pasa
# -DPRUEBA_PWM=ON) se compila prueba_pwm.c en su lugar.
if(PRUEBA_PWM)
    set(fuentes "prueba_pwm.c")
else()
    set(fuentes "main.c")
endif()

# Los requisitos NO pueden depender de PRUEBA_PWM: ESP-IDF los lee en una
# primera pasada en la que las variables -D todavía no existen. Por eso se
# declaran los de ambos programas; lo que uno no usa no se enlaza.
idf_component_register(
    SRCS ${fuentes}
    INCLUDE_DIRS "."
    PRIV_REQUIRES config motores consola estado control red
                  nvs_flash esp_driver_ledc esp_driver_gpio
)
