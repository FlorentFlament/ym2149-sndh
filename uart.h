void uart_init(void);
char uart_get_byte(void);
void uart_put_byte(char data);
void uart_write_string(const char *buf);
void uart_read_string(char* buf);
