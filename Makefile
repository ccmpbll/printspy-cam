.PHONY: build flash monitor menuconfig clean

build:
	idf.py build

flash:
	idf.py flash

monitor:
	idf.py monitor

menuconfig:
	idf.py menuconfig

clean:
	idf.py fullclean
