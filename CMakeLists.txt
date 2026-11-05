cmake_minimum_required(VERSION 3.22)
 
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
 
# En la prueba del osciloscopio se permite el rango completo de 0 a 100 %
if(PRUEBA_PWM)
    idf_build_set_property(COMPILE_DEFINITIONS "MOTOR_LIMITE_PORCENTAJE=100.0f" APPEND)
endif()

project(IE-0624-Proyecto)