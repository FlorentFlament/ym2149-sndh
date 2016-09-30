#include <util/delay.h>

#include "blink.h"

void init_led(void) {
  DDRB |= 1<<LED;
  DDRB |= 1<<PORTB0 | 1<<PORTB1 | 11<<PORTB2;
}

void blink(void) {
  PORTB |= (1 << LED);
  _delay_ms(BLINK_SPEED);
  PORTB &= (0 << LED);
  _delay_ms(BLINK_SPEED);
}

void blink_n(int count) {
  int i;
  for (i=0; i<count; i++) {
    blink();
  }
  _delay_ms(2*BLINK_SPEED);
}

void set_leds(int n) {
  PORTB |= n & 0x07;
  PORTB &= 0xf8 | (n&0x07);
}
