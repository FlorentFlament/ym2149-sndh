#include <avr/sleep.h>

#include "circ_buffer.h"

int circ_buf_len(const struct circular_buffer* buf) {
  if (buf->end < buf->start) {
    return (buf->end - buf->start) + buf->size;
  }
  return buf->end - buf->start;
}

int circ_buf_full(const struct circular_buffer* buf) {
  if (buf->end == buf->data + buf->size - 1) {
    return buf->start == 0;
  }
  return buf->end == buf->start - 1;
}

int circ_buf_empty(const struct circular_buffer* buf) {
  return buf->end == buf->start;
}

void __circ_buf_put_byte_nb(struct circular_buffer* buf, char c) {
  *(buf->end) = c;
  if (++buf->end == buf->data + buf->size) {
    buf->end = buf->data;
  }
}

char __circ_buf_get_byte_nb(struct circular_buffer* buf) {
  char c = *(buf->start);
  if (++buf->start == buf->data + buf->size) {
    buf->start = buf->data;
  }
  return c;
}

void circ_buf_put_byte(struct circular_buffer* buf, char c) {
  while (circ_buf_full(buf)) {
    sleep_cpu();
  }
  __circ_buf_put_byte_nb(buf, c);
}

char circ_buf_get_byte(struct circular_buffer* buf) {
  while (circ_buf_empty(buf)) {
    sleep_cpu();
  }
  return __circ_buf_get_byte_nb(buf);
}
