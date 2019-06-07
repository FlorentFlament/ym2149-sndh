avrType=atmega328p
avrFreq=16000000 # 16 Mhz
programmerDev=/dev/ttyUSB0
programmerType=arduino

cflags=-DF_CPU=$(avrFreq) -mmcu=$(avrType) -Wall -Werror -Wextra -Ofast

objects=$(patsubst %.c,%.o,$(wildcard *.c))


.PHONY: clean flash

all: firmware.hex

uart.o: uart.c uart.h

%.o: %.c
	avr-gcc $(cflags) -c $< -o $@

firmware.elf: $(objects)
	avr-gcc $(cflags) -o $@ $^

firmware.hex: firmware.elf
	avr-objcopy -j .text -j .data -O ihex $^ $@

flash: firmware.hex
	avrdude -p$(avrType) -c$(programmerType) -P$(programmerDev) -v -U flash:w:$< -D -b57600

firmware-fast.hex: firmware-fast.asm
	avra $<

flash-fast: firmware-fast.hex
	avrdude -p$(avrType) -c$(programmerType) -P$(programmerDev) -v -U flash:w:$< -D -b57600

clean:
	rm -f firmware.hex firmware.elf \
	      firmware-fast.hex firmware-fast.cof firmware-fast.obj firmware-fast.eep.hex \
	      $(objects) *.s
