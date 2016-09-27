// We need to instruct the compiler that these data may be changed
// during an interrupt.
struct circular_buffer {
  volatile char* start;
  volatile char* end;
  volatile char* volatile first;
  volatile char* volatile last;
};

int circ_buf_len(const struct circular_buffer* buf);

int circ_buf_free(const struct circular_buffer* buf);

int circ_buf_full(const struct circular_buffer* buf);

int circ_buf_empty(const struct circular_buffer* buf);

void __circ_buf_put_byte_nb(struct circular_buffer* buf, char c);

char __circ_buf_get_byte_nb(struct circular_buffer* buf);


// These function sleeps if buffer is full
// Do not use in interrupts

void circ_buf_put_byte(struct circular_buffer* buf, char c);

char circ_buf_get_byte(struct circular_buffer* buf);
