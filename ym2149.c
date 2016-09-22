#include <util/delay.h>

#include "ym2149.h"

// MSB (PB3) is connected to BDIR
// LSB (PB2) is connected to BC1
// +5V is connected to BC2
#define DATA_READ (0x01 << 2)
#define DATA_WRITE (0x02 << 2)
#define ADDRESS_MODE (0x03 << 2)

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
  DDRC |= 0x0c; // Bits 2 and 3
}
void set_data_out(void) {
  DDRC |= 0x03; // Bits 0 and 1
  DDRD |= 0xfc; // Bits 2 to 7
}
void set_data_in(void) {
  DDRC &= ~0x03; // Bits 0 and 1
  DDRD &= ~0xfc; // Bits 2 to 7
}

void set_address(char addr) {
  set_data_out();
  PORTC = (PORTC & 0xf3) | ADDRESS_MODE;
  PORTC = (PORTC & 0xfc) | (addr & 0x03); // 2 first bits ont PORTC
  PORTD = (PORTD & 0x02) | (addr & 0xfc); // 6 last bits on PORTD
  _delay_us(1.); //tAS = 300ns
  PORTC = (PORTC & 0xf3) /*INACTIVE*/ ;
  _delay_us(1.); //tAH = 80ns
}

void set_data(char data) {
  set_data_out();
  PORTC = (PORTC & 0xfc) | (data & 0x03); // 2 first bits ont PORTC
  PORTD = (PORTD & 0x02) | (data & 0xfc); // 6 last bits on PORTD
  PORTC = (PORTC & 0xf3) | DATA_WRITE;
  _delay_us(1.); // 300ns < tDW < 10us
  PORTC = (PORTC & 0xf3) /*INACTIVE*/ ; // To fit tDW max
  _delay_us(1.); // tDH = 80ns
}

char get_data(void) {
  char data;
  set_data_in();
  PORTC = (PORTC & 0xf3) | DATA_READ;
  _delay_us(1.); // tDA = 400ns
  data = (PIND & 0xfc) | (PINB & 0x03);
  PORTC = (PORTC & 0xf3) /*INACTIVE*/ ;
  _delay_us(1.); // tTS = 100ns
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
