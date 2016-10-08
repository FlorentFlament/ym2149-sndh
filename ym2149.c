#include <util/delay.h>

#include "ym2149.h"

// MSB (PB5) is connected to BDIR
// LSB (PB4) is connected to BC1
// +5V is connected to BC2
#define DATA_READ (0x01 << 4)
#define DATA_WRITE (0x02 << 4)
#define ADDRESS_MODE (0x03 << 4)

// Sets a 4MHz clock OC2A (PORTB3)
void ym_set_clock(void) {
  DDRB |= 0x01 << PORTB3;
  // Toggle OC2A on compare match
  TCCR2A &= ~(0x01 << COM2A1);
  TCCR2A |=   0x01 << COM2A0;
  // Clear Timer on Compare match
  TCCR2B &= ~(0x01 << WGM22);
  TCCR2A |=   0x01 << WGM21;
  TCCR2A &= ~(0x01 << WGM20);
  // Use CLK I/O without prescaling
  TCCR2B &= ~(0x01 << CS22);
  TCCR2B &= ~(0x01 << CS21);
  TCCR2B |=   0x01 << CS20;
  // Divide the 16MHz clock by 8 -> 2MHz
  OCR2A = 3;
}

void ym_set_bus_ctl(void) {
  DDRB |= 0x30; // Bits 4 and 5
}
void set_data_out(void) {
  DDRC |= 0x3f; // Bits 0 to 5
  DDRD |= 0xc0; // Bits 6 to 7
}
void set_data_in(void) {
  DDRC &= ~0x3f; // Bits 0 to 5
  DDRD &= ~0xc0; // Bits 6 to 7
}

void set_address(char addr) {
  set_data_out();
  PORTB = (PORTB & 0xcf) | ADDRESS_MODE;
  PORTC = (PORTC & 0xc0) | (addr & 0x3f); // 6 first bits ont PORTC
  PORTD = (PORTD & 0x3f) | (addr & 0xc0); // 2 last bits on PORTD
  //tAS = 300ns = 4.8 clock cycles
  PORTB = (PORTB & 0xcf) /*INACTIVE*/ ;
  //tAH = 80ns  = 1.3 clock cycles
}

void set_data(char data) {
  set_data_out();
  PORTC = (PORTC & 0xc0) | (data & 0x3f); // 6 first bits ont PORTC
  PORTD = (PORTD & 0x3f) | (data & 0xc0); // 2 last bits on PORTD
  PORTB = (PORTB & 0xcf) | DATA_WRITE;
  // 300ns < tDW < 10us = 4.8 clock cycles
  PORTB = (PORTB & 0xcf) /*INACTIVE*/ ; // To fit tDW max
  // tDH = 80ns = 1.3 clock cycles
}

char get_data(void) {
  char data;
  set_data_in();
  PORTB = (PORTB & 0xcf) | DATA_READ;
  // tDA = 400ns = 6.4 clock cycles
  // The 16 cycles delay may be required there
  _delay_us(1.);
  data = (PINC & 0x3f) | (PIND & 0xc0);
  PORTB = (PORTB & 0xcf) /*INACTIVE*/ ;
  // tTS = 100ns = 1.6 clock cycles
  return data;
}

void ym_send_data(char addr, char data) {
  set_address(addr);
  set_data(data);
}

char ym_read_data(char addr) {
  set_address(addr);
  return get_data();
}
