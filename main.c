#include <avr/interrupt.h>

#include "uart.h"
#include "circ_buffer.h"

#define BUF_SIZE 1024

// Possible RX states
#define RX_PROGRESS 1
#define RX_COMPLETE 2
#define RX_WAITING  3

static char buf_data[BUF_SIZE];
static struct circular_buffer buf;

static volatile int state;

// Define interrupt code when data is received from the UART
ISR(USART_RX_vect) {
  static int count;
  static int chunk_size;
  switch(state) {
  case RX_WAITING:
    count = 0;
    chunk_size = UDR0;
    state = RX_PROGRESS;
    break;
  case RX_PROGRESS:
    __circ_buf_put_byte_nb(&buf, UDR0);
    if (++count >= chunk_size) {
      state = RX_COMPLETE;
    }
  }
}

void init(void) {
  buf.size  = BUF_SIZE;
  buf.data  = buf_data;
  buf.start = buf_data;
  buf.end   = buf_data;

  state = RX_WAITING;
  uart_init();

  // Enable interrupts
  UCSR0B |= 1 << RXCIE0; // Enable USART RX complete interrupt
  sei();
}

int main() {
  init();

  // Separating output from previously buffered data
  uart_write_string("*** START ***");
  for(;;) {
    if (state == RX_COMPLETE) {
      uart_put_byte('*');
      uart_put_byte(' ');
      while (!circ_buf_empty(&buf)) {
      	uart_put_byte(__circ_buf_get_byte_nb(&buf));
      }
      state = RX_WAITING;
    }
  }
}
