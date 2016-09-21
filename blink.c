#include <util/delay.h>

#include "blink.h"

void init_led(void) {
  DDRB |= 1 << LED;
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
