#include <avr/interrupt.h>

#include "circ_buffer.h"
#include "ym2149.h"
#include "blink.h"

#define BUF_SIZE 1024

// Possible RX states
#define RX_PROGRESS 1
#define RX_COMPLETE 2
#define RX_WAITING  3

// Possible statuses sent on the UART
#define RX_ACK 0
#define RX_BAD 255

// Possible Samples states
#define SMP_TSHI 0
#define SMP_TSLO 1
#define SMP_WAIT 3
#define SMP_ADDR 4
#define SMP_VAL 5
#define SMP_ERR 6

// Macro to get a byte from a circular buffer
#define M_CIRC_BUF_GET_BYTE *buf.first; if(++buf.first==buf.end){buf.first=buf.start;}
#define M_BLK_CB_GET_BYTE(VAR) while(buf.last==buf.first); VAR=M_CIRC_BUF_GET_BYTE
#define M_UART_PUT_BYTE(DATA) while(!(UCSR0A & (1<<UDRE0))); UDR0=DATA

static char buf_data[BUF_SIZE];
static struct circular_buffer buf;

static volatile unsigned char rx_state; // RX State register
static unsigned char smp_state; // Sample state register

static volatile unsigned int free_cnt; // Free space in buffer

// Define interrupt code when data is received from the UART
ISR(USART_RX_vect) {
  static unsigned char count;
  static unsigned char chunk_size;
  unsigned char flags;

  switch(rx_state) {
  case RX_PROGRESS:
    *buf.last = UDR0;
    if (++buf.last == buf.end) buf.last = buf.start;
    if (++count == chunk_size) rx_state = RX_COMPLETE;
    break;
  case RX_WAITING:
    flags = UCSR0A;
    chunk_size = UDR0; // consuming data
    if (flags & 1<<FE0) { // Frame Error (4)
      M_UART_PUT_BYTE(FE0);
      M_UART_PUT_BYTE(chunk_size);
      rx_state = RX_COMPLETE;
      break;
    }
    if (flags & 1<<DOR0) { // Data OverRun (3)
      M_UART_PUT_BYTE(DOR0);
      M_UART_PUT_BYTE(chunk_size);
      rx_state = RX_COMPLETE;
      break;
    }
    if (flags & 1<<UPE0) { // Parity Error (2)
      M_UART_PUT_BYTE(UPE0|0xf0);
      M_UART_PUT_BYTE(chunk_size);
      rx_state = RX_COMPLETE;
      break;
    }
    if (chunk_size > free_cnt) {
      M_UART_PUT_BYTE(RX_BAD);
      rx_state = RX_COMPLETE;
      break;
    }
    M_UART_PUT_BYTE(RX_ACK);
    if (chunk_size == 0) {
      rx_state = RX_COMPLETE;
      break;
    }
    count = 0;
    rx_state = RX_PROGRESS;
  }
}


/***** Initialization *****/

void init_uart(void) {
  // Set max speed i.e 1Mbits
  UBRR0H = 0;
  UBRR0L = 0;
  // Setting bi-directionnal UART communication
  // Enable USART RX complete interrupt
  UCSR0B |= 1<<TXEN0 | 1<<RXEN0 | 1<<RXCIE0;
  // Set frame format = 8-N-1 with Odd Parity enabled
  UCSR0C = 0x03<<UCSZ00;// | 0x02<<UPM00;
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
  sei();
}


/***** Main *****/

int main() {
  unsigned char addr=0;
  unsigned char val;

  init();
  smp_state = SMP_TSHI;
  rx_state = RX_COMPLETE;

  for(;;) {
    set_leds(rx_state);
    // Sample state machine
    switch(smp_state) {
    case SMP_TSHI:
      // Set OCR1A to the next timestamp
      if (buf.last == buf.first) break;
      OCR1AH = M_CIRC_BUF_GET_BYTE;
      smp_state = SMP_TSLO;
    case SMP_TSLO:
      if (buf.last == buf.first) break;
      OCR1AL = M_CIRC_BUF_GET_BYTE;
      TIFR1 |= 1<<OCF1A; // Clear the flag
      smp_state = SMP_WAIT;
    case SMP_WAIT:
      if (!(TIFR1 & 1<<OCF1A)) break;
      smp_state = SMP_ADDR;
    case SMP_ADDR:
      if (buf.last == buf.first) break;
      addr = M_CIRC_BUF_GET_BYTE;
      if (addr == 0xff) { smp_state = SMP_TSHI; break; } // End of sample
      if (addr & 0xf0) { smp_state = SMP_ERR; break; } // Address is bogus
      smp_state = SMP_VAL; // Address is valid
    case SMP_VAL:
      if (buf.last == buf.first) break;
      val = M_CIRC_BUF_GET_BYTE;
      ym_send_data(addr, val);
      smp_state = SMP_ADDR;
      break;
    default: /* SMP_ERR */
      if (buf.last == buf.first) break;
      addr = M_CIRC_BUF_GET_BYTE;
      if (addr != 0xff) break; // Consuming data until next sample
      smp_state = SMP_TSHI;
    }

    // RX State machine when RX_COMPLETE
    if (rx_state == RX_COMPLETE) {
      if (buf.last < buf.first) free_cnt = buf.first - buf.last - 1;
      else free_cnt = BUF_SIZE-1 - (buf.last - buf.first);
      if ((free_cnt > 0) && (UCSR0A & 1<<UDRE0)) {
	if (free_cnt > 255) UDR0 = 255;
	else UDR0 = free_cnt;
	rx_state = RX_WAITING;
      }
    }
  }
}
