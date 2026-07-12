.PHONY: build flash monitor menuconfig clean

# Board-specific settings (board select, flash size, anything else that
# genuinely differs) live in board-configs/<BOARD>.sdkconfig - see that
# directory for the available boards. Switching BOARD on a build directory
# already configured for a different board needs `make clean` first -
# SDKCONFIG_DEFAULTS only applies when sdkconfig is (re)generated, an
# existing sdkconfig from a prior board silently wins otherwise.
SDKCONFIG_DEFAULTS = sdkconfig.defaults;sdkconfig.defaults.esp32s3;board-configs/$(BOARD).sdkconfig

build:
ifndef BOARD
	$(error BOARD is required, e.g. make build BOARD=nulllab_esp32s3_cam - see board-configs/ for available boards)
endif
	idf.py -D IDF_TARGET=esp32s3 -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" build

flash:
ifndef BOARD
	$(error BOARD is required, e.g. make flash BOARD=nulllab_esp32s3_cam)
endif
	idf.py -D IDF_TARGET=esp32s3 -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" flash

monitor:
	idf.py monitor

menuconfig:
ifndef BOARD
	$(error BOARD is required, e.g. make menuconfig BOARD=nulllab_esp32s3_cam)
endif
	idf.py -D IDF_TARGET=esp32s3 -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" menuconfig

clean:
	idf.py fullclean
