avrType=atmega328p
avrFreq=16000000 # 16 Mhz
programmerDev=/dev/ttyUSB0
programmerType=arduino

#cflags=-DF_CPU=$(avrFreq) -mmcu=$(avrType) -Wall -Werror -Wextra -Ofast
cflags=-DF_CPU=$(avrFreq) -mmcu=$(avrType) -Wall -Werror -Wextra -Os
#cflags=-DF_CPU=$(avrFreq) -mmcu=$(avrType) -Wall -Wextra -O0

objects=$(patsubst %.c,%.o,$(wildcard *.c))


.PHONY: clean flash

all: main.hex

uart.o: uart.c uart.h

%.o: %.c
	avr-gcc $(cflags) -c $< -o $@

main.elf: $(objects)
	avr-gcc $(cflags) -o $@ $^

main.hex: main.elf
	avr-objcopy -j .text -j .data -O ihex $^ $@

flash: main.hex
	avrdude -p$(avrType) -c$(programmerType) -P$(programmerDev) -v -U flash:w:$< -D

clean:
	rm -f main.hex main.elf $(objects)
