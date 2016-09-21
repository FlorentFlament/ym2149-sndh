#include <avr/io.h>

#include "uart.h"

void uart_init(void) {
  // Set max speed i.e 1Mbits
  // Tested Double Transmission speed (U2X0) but getting random errors on the line
  // So staying at 1Mbits for now
  UBRR0H = 0;
  UBRR0L = 0;

  // Set frame format = 8-N-1
  UCSR0C = (0x03 << UCSZ00);

  // Setting bi-directionnal UART communication
  UCSR0B |= (1 << TXEN0) | (1 << RXEN0);
}

char uart_get_byte(void) {
  while (!(UCSR0A & (1<<RXC0)));
  return UDR0;
}

void uart_put_byte(char data) {
  while (!(UCSR0A & (1<<UDRE0)));
  UDR0 = data;
}

void uart_write_string(const char *str) {
  while (*str != '\0') {
    uart_put_byte(*str);
    str++;
  }
  uart_put_byte('\n');
}

void uart_read_string(char* buf) {
  while((*buf = uart_get_byte()) != '\n') {
    buf++;
  }
  *buf = '\0';
}
