idf_component_register(
    SRCS "motores.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES config esp_driver_ledc esp_driver_gpio
)