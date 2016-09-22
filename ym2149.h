#include <avr/io.h>

#define YM_REGS_CNT 16

void ym_set_clock(void);
void ym_set_bus_ctl(void);

void ym_send_data(char addr, char data);
char ym_read_data(char addr);
