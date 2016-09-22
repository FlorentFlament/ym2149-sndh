#include <avr/interrupt.h>

#include "circ_buffer.h"
#include "ym2149.h"
#include "blink.h"

#define BUF_SIZE 1024

// Possible RX states
#define RX_PROGRESS 1
#define RX_COMPLETE 2
#define RX_WAITING  3

#define SP_PROCESS 1
#define SP_WAITING 2

struct ym_sample {
  volatile char reg_enabled[YM_REGS_CNT];
  volatile char reg_values[YM_REGS_CNT];
};

static char buf_data[BUF_SIZE];
static struct circular_buffer buf;
static struct ym_sample sample;

// States registers
static volatile int rx_state; // UART RX state
static volatile int sp_state; // Sample state


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

// Send sample to YM2149 at the right time
ISR(TIMER1_COMPA_vect) {
  sp_state = SP_PROCESS;
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

void init_samples_timer(void) {
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

void init_sample(void) {
  int i;
  for (i=0; i<YM_REGS_CNT; i++) {
    sample.reg_enabled[i] = 0;
    sample.reg_values[i] = 0;
  }
}

void init(void) {
  buf.size  = BUF_SIZE;
  buf.data  = buf_data;
  buf.start = buf_data;
  buf.end   = buf_data;

  rx_state = RX_COMPLETE;
  sp_state = SP_PROCESS;

  init_led();
  init_uart();
  init_sample();
  init_samples_timer();

  ym_set_clock();
  ym_set_bus_ctl();

  // Enable interrupts
  UCSR0B |= 1 << RXCIE0; // Enable USART RX complete interrupt
  TIMSK1 |= 1 << OCIE1A; // Enable Timer/Counter1, Output Compare A Match Interrupt
  sei();
}


/***** Some utility functions *****/

void get_next_sample(void) {
  int i;
  int count;
  unsigned char addr;

  // Set OCR1A to the next timestamp
  OCR1AH = circ_buf_get_byte(&buf);
  OCR1AL = circ_buf_get_byte(&buf);

  // Don't update registers if not required
  for (i=0; i<YM_REGS_CNT; i++) {
    sample.reg_enabled[i] = 0;
  }

  // Read registers sent
  count = circ_buf_get_byte(&buf);
  if (count <= YM_REGS_CNT) {
    // Sanity check
    for (i=0; i<count; i++) {
      addr = circ_buf_get_byte(&buf);
      if (addr < YM_REGS_CNT) {
        // Sanity check
        sample.reg_enabled[addr] = 1;
        sample.reg_values[addr] = circ_buf_get_byte(&buf);
      }
    }
  }
}

void play_sample(void) {
  int i;
  for (i=0; i<YM_REGS_CNT; i++) {
    if (sample.reg_enabled[i]) {
      ym_send_data(i, sample.reg_values[i]);
    }
  }
}


/***** Main *****/

int main() {
  init();

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
    if (sp_state == SP_PROCESS) {
      play_sample();
      get_next_sample();
      sp_state = SP_WAITING;
    }
  }
}
