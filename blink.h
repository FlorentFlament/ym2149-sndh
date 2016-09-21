#include <avr/io.h>

#define LED PORTB5
#define BLINK_SPEED 150 // ms

void init_led(void);
void blink(void);
void blink_n(int count);
