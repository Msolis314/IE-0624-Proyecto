idf_component_register(
    SRCS "control.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES estado motores config esp_timer
)