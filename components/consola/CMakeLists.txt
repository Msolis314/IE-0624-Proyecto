idf_component_register(
    SRCS "consola.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_usb_serial_jtag
)