#include <avr/sleep.h>

#include "circ_buffer.h"

int circ_buf_len(const struct circular_buffer* buf) {
  return buf->last - buf->first - circ_buf_free(buf);
}

int circ_buf_free(const struct circular_buffer* buf) {
  if (buf->last < buf->first) {
    return buf->first - buf->last - 1;
  }
  return (buf->end - buf->start) - (buf->last - buf->first);
}

int circ_buf_full(const struct circular_buffer* buf) {
  if (buf->last == (buf->end - 1)) {
    return buf->first == 0;
  }
  return buf->last == (buf->first - 1);
}

int circ_buf_empty(const struct circular_buffer* buf) {
  return buf->last == buf->first;
}

void __circ_buf_put_byte_nb(struct circular_buffer* buf, char c) {
  *(buf->last) = c;
  if (++buf->last == buf->end) {
    buf->last = buf->start;
  }
}

char __circ_buf_get_byte_nb(struct circular_buffer* buf) {
  char c = *(buf->first);
  if (++buf->first == buf->end) {
    buf->first = buf->start;
  }
  return c;
}

void circ_buf_put_byte(struct circular_buffer* buf, char c) {
  while (circ_buf_full(buf));
  __circ_buf_put_byte_nb(buf, c);
}

char circ_buf_get_byte(struct circular_buffer* buf) {
  while (circ_buf_empty(buf));
  return __circ_buf_get_byte_nb(buf);
}
