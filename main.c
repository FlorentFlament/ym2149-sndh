#include <avr/interrupt.h>

#include "circ_buffer.h"
#include "ym2149.h"
#include "blink.h"

#define BUF_SIZE 1024

// Possible RX states
#define RX_PROGRESS 1
#define RX_COMPLETE 2
#define RX_WAITING  3

// Macro to get a byte from a circular buffer
#define M_CIRC_BUF_GET_BYTE *buf.first; if(++buf.first==buf.end){buf.first=buf.start;}
#define M_BLK_CB_GET_BYTE(VAR) while(buf.last==buf.first); VAR=M_CIRC_BUF_GET_BYTE

static char buf_data[BUF_SIZE];
static struct circular_buffer buf;

// States registers
static volatile unsigned char rx_state; // UART RX state

/***** Interruption vectors *****/

// Define interrupt code when data is received from the UART
ISR(USART_RX_vect) {
  static unsigned char count;
  static unsigned char chunk_size;
  switch(rx_state) {
  case RX_WAITING:
    count = 0;
    chunk_size = UDR0;
    rx_state = RX_PROGRESS;
    break;
  case RX_PROGRESS:
    *buf.last = UDR0;
    if (++buf.last == buf.end) {
      buf.last = buf.start;
    }
    if (++count == chunk_size) {
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
  buf.end  = buf_data + BUF_SIZE;
  buf.start  = buf_data;
  buf.first = buf_data;
  buf.last   = buf_data;

  init_led();
  init_uart();
  init_timer();

  ym_set_clock();
  ym_set_bus_ctl();

  // Enable interrupts
  UCSR0B |= 1 << RXCIE0; // Enable USART RX complete interrupt
  sei();
}


/***** Main *****/

int main() {
  init();

  // Bootstrap stream
  UDR0 = 255;
  rx_state = RX_WAITING;
  // Set OCR1A to the next timestamp
  M_BLK_CB_GET_BYTE(OCR1AH);
  M_BLK_CB_GET_BYTE(OCR1AL);

  for(;;) {
    if (rx_state == RX_COMPLETE) {
      // Empty space in circular buffer
      int n;
      if (buf.last < buf.first) n = buf.first - buf.last - 1;
      else n = BUF_SIZE-1 - (buf.last - buf.first);

      if (n > 0) {
	if (n > 255) UDR0 = 255;
	else UDR0 = n;
        rx_state = RX_WAITING;
      }
    }

    if (TIFR1 & 1<<OCF1A) {
      unsigned char i;
      unsigned char count;
      M_BLK_CB_GET_BYTE(count);
      for (i=0; i < count; i++) {
        unsigned char addr;
        char val;
        M_BLK_CB_GET_BYTE(addr);
        M_BLK_CB_GET_BYTE(val);
        ym_send_data(addr, val);
      }
      // Set OCR1A to the next timestamp
      M_BLK_CB_GET_BYTE(OCR1AH);
      M_BLK_CB_GET_BYTE(OCR1AL);
      TIFR1 |= 1<<OCF1A; // Clear the flag
    }
  }
}
