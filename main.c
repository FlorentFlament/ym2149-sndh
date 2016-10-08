#include <avr/interrupt.h>

#include "circ_buffer.h"
#include "ym2149.h"
#include "blink.h"

#define BUF_SIZE 1536

// Possible RX states
#define RX_PROGRESS 1
#define RX_COMPLETE 2
#define RX_WAITING  3
#define RX_CHUNKLO  4
#define RX_WAITLO   5

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

// Sound level is an exponential function of a channel's volume
// Formula is V = exp(ln(2)/2 * (x-15)) were x is the volume in the YM2149
// Exact formula to compute the array:
// [round(256*math.exp((math.log(2)/2)*(x-15))) - 1 for x in range(16)]
static unsigned char sound_level[] = {0, 1, 2, 3, 5, 7, 10, 15, 22,
				      31, 44, 63, 90, 127, 180, 255};

// Define interrupt code when data is received from the UART
ISR(USART_RX_vect) {
  static unsigned int count;
  static unsigned int chunk_size;

  switch(rx_state) {
  case RX_PROGRESS:
    *buf.last = UDR0;
    if (++buf.last == buf.end) buf.last = buf.start;
    if (++count == chunk_size) rx_state = RX_COMPLETE;
    break;
  case RX_WAITING:
    chunk_size = UDR0; // consuming data
    rx_state = RX_WAITLO;
    break;
  case RX_WAITLO:
    chunk_size = (chunk_size<<8) + UDR0;
    if (chunk_size > free_cnt) {
      UDR0 = RX_BAD;
      rx_state = RX_COMPLETE;
      break;
    }
    UDR0 = RX_ACK;
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
  // Set frame format = 8-N-1 - Only supported format by ch341 linux driver
  UCSR0C = 0x03<<UCSZ00;
}

void init_samples_timer(void) {
  // Let OC1A in normal port operation (disconnected) / Normal counting mode
  // Prescale CLK I/O from 16MHz to 2MHz
  TCCR1B |= 1<<CS11;
}

void init_pwm(void) {
  // Set OC0B in fast pwm mode
  TCCR0A |= 0x02<<COM0B0; // Clear OC0B on Compare Match, set OC0B at BOTTOM, (non-inverting mode)
  TCCR0A |= 0x03<<WGM00;  // Fast PWM
  TCCR0B |= 0x01<<CS00;   // clkI/O /(No prescaling)
  // Set OC2A & OC2B in fast pwm mode
  TCCR2A |= 0x02<<COM2A0; // Clear OC2A on Compare Match, set OC2A at BOTTOM, (non-inverting mode)
  TCCR2A |= 0x02<<COM2B0; // Clear OC2B on Compare Match, set OC2B at BOTTOM, (non-inverting mode)
  TCCR2A |= 0x03<<WGM20;  // Fast PWM
  TCCR2B |= 0x01<<CS20;   // clkI/O /(No prescaling)
  // Toggle Pins to output
  DDRD |= 1<<DDD5; // Set OC0B
  DDRB |= 1<<DDB3; // Set OC2A
  DDRD |= 1<<DDD3; // Set OC2B
}

void init(void) {
  buf.end  = buf_data + BUF_SIZE;
  buf.start  = buf_data;
  buf.first = buf_data;
  buf.last   = buf_data;

  // init_led();
  init_uart();
  init_samples_timer();
  init_pwm();

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
      // Have LED aligned with 4 LSB @ addresses 0x8 0x9 & 0xa
      switch (addr) {
      case 0x8:	OCR0B = sound_level[val&0x0f]; break;
      case 0x9:	OCR2A = sound_level[val&0x0f]; break;
      case 0xa:	OCR2B = sound_level[val&0x0f];
      }
      smp_state = SMP_ADDR;
      break;
    default: /* SMP_ERR */
      if (buf.last == buf.first) break;
      addr = M_CIRC_BUF_GET_BYTE;
      if (addr != 0xff) break; // Consuming data until next sample
      smp_state = SMP_TSHI;
    }

    // RX State machine when RX_COMPLETE
    switch (rx_state) {
    case RX_COMPLETE:
      if (!(UCSR0A & 1<<UDRE0)) break;
      if (buf.last < buf.first) free_cnt = buf.first - buf.last - 1;
      else free_cnt = BUF_SIZE-1 - (buf.last - buf.first);
      if (free_cnt == 0) break;
      UDR0 = free_cnt>>8 & 0xff;
      rx_state = RX_CHUNKLO;
    case RX_CHUNKLO:
      if (!(UCSR0A & 1<<UDRE0)) break;
      UDR0 = free_cnt & 0xff;
      rx_state = RX_WAITING;
    }
  }
}
