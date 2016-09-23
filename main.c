#include <avr/interrupt.h>

#include "circ_buffer.h"
#include "ym2149.h"
#include "blink.h"

#define BUF_SIZE 1024

// Possible RX states
#define RX_PROGRESS 1
#define RX_COMPLETE 2
#define RX_WAITING  3

static char buf_data[BUF_SIZE];
static struct circular_buffer buf;

// States registers
static volatile int rx_state; // UART RX state

/***** Interruption vectors *****/

// Define interrupt code when data is received from the UART
ISR(USART_RX_vect) {
  static int count;
  static int chunk_size;
  switch(rx_state) {
  case RX_WAITING:
    count = 0;
    chunk_size = UDR0;
    rx_state = RX_PROGRESS;
    break;
  case RX_PROGRESS:
    __circ_buf_put_byte_nb(&buf, UDR0);
    if (++count >= chunk_size) {
      rx_state = RX_COMPLETE;
    }
  }
}


/***** Initialization *****/

void init_uart(void) {
  // Set max speed i.e 1Mbits
  UBRR0H = 0;
  UBRR0L = 0;
  // Set frame format = 8-N-1
  UCSR0C = (0x03 << UCSZ00);
  // Setting bi-directionnal UART communication
  UCSR0B |= (1 << TXEN0) | (1 << RXEN0);
}

void init_timer(void) {
  // Set OC1A in normal port operation (disconnected)
  TCCR1A &= ~(0x01 << COM1A1);
  TCCR1A &= ~(0x01 << COM1A0);
  // Normal counting mode
  TCCR1B &= ~(0x01 << WGM13);
  TCCR1B &= ~(0x01 << WGM12);
  TCCR1A &= ~(0x01 << WGM11);
  TCCR1A &= ~(0x01 << WGM10);
  // Prescale CLK I/O from 16MHz to 2MHz
  TCCR1B &= ~(0x01 << CS12);
  TCCR1B |=   0x01 << CS11;
  TCCR1B &= ~(0x01 << CS10);
}

void init(void) {
  buf.size  = BUF_SIZE;
  buf.data  = buf_data;
  buf.start = buf_data;
  buf.end   = buf_data;

  init_led();
  init_uart();
  init_timer();

  ym_set_clock();
  ym_set_bus_ctl();

  // Enable interrupts
  UCSR0B |= 1 << RXCIE0; // Enable USART RX complete interrupt
  sei();
}


/***** Some utility functions *****/

void update_timer(void) {
  // Set OCR1A to the next timestamp
  OCR1AH = circ_buf_get_byte(&buf);
  OCR1AL = circ_buf_get_byte(&buf);
}

void play_sample(void) {
  int i;
  unsigned char count = circ_buf_get_byte(&buf);
  for (i=0; i < count; i++) {
    unsigned char addr = circ_buf_get_byte(&buf);
    ym_send_data(addr, circ_buf_get_byte(&buf));
  }
}

void bootstrap_stream(void) {
  // Ask for 255 bytes of data
  UDR0 = 255;
  rx_state = RX_WAITING;
  update_timer();
}

/***** Main *****/

int main() {
  init();
  bootstrap_stream();

  for(;;) {
    if (rx_state == RX_COMPLETE) {
      int n = circ_buf_free(&buf);
      if (n > 0) {
	if (n > 255) {
	  UDR0 = 255;
	} else {
	  UDR0 = n;
	}
	rx_state = RX_WAITING;
      }
    }
    if (TIFR1 & 1<<OCF1A) {
      play_sample();
      update_timer();
      TIFR1 |= 1<<OCF1A; // Clear the flag
    }
  }
}
