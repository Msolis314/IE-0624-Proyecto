# Makefile envoltorio para ESP-IDF (idf.py)
# Proyecto: control inalambrico y seguimiento de luz, plataforma Junior
#
# Uso:
#   make                 compila
#   make flash           compila y flashea
#   make monitor         abre el monitor serial (salir con Ctrl+])
#   make fm              flashea y abre el monitor
#   make menuconfig      configuracion del proyecto
#   make PORT=/dev/ttyUSB0 flash   usar otro puerto

SHELL := /bin/bash

# --- Configuracion -------------------------------------------------------
IDF_VERSION  ?= v6.1
IDF_ACTIVATE ?= $(HOME)/.espressif/tools/activate_idf_$(IDF_VERSION).sh
TARGET       ?= esp32s3
PORT         ?= /dev/ttyACM0
BAUD         ?= 921600

# Si el entorno ya esta activado (IDF_PATH definido) se usa directamente;
# si no, se activa en cada receta. Cada linea de una receta corre en su
# propio shell, por eso el source va en la misma linea que idf.py.
ifdef IDF_PATH
  IDF := idf.py
else
  IDF := source "$(IDF_ACTIVATE)" > /dev/null && idf.py
endif

.PHONY: all build flash monitor fm menuconfig size clean fullclean erase set-target check-env help

all: build

build: check-env
	$(IDF) build

flash: check-env
	$(IDF) -p $(PORT) -b $(BAUD) flash

monitor: check-env
	$(IDF) -p $(PORT) monitor

fm: check-env
	$(IDF) -p $(PORT) -b $(BAUD) flash monitor

menuconfig: check-env
	$(IDF) menuconfig

size: check-env
	$(IDF) size

set-target: check-env
	$(IDF) set-target $(TARGET)

clean: check-env
	$(IDF) clean

fullclean: check-env
	$(IDF) fullclean

erase: check-env
	$(IDF) -p $(PORT) erase-flash

check-env:
ifndef IDF_PATH
	@test -f "$(IDF_ACTIVATE)" || { \
	  echo "No se encontro $(IDF_ACTIVATE)."; \
	  echo "Ajuste IDF_VERSION o IDF_ACTIVATE, o active el entorno antes de llamar a make."; \
	  exit 1; }
endif

help:
	@grep -E '^#   make' $(MAKEFILE_LIST) | sed 's/^#   //'
