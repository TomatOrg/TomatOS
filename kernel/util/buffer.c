#include "buffer.h"

#include "printf.h"
#include "stb_ds.h"

#include <mem/malloc.h>

#include <stdarg.h>

buffer_t* create_buffer() {
    buffer_t* buffer = malloc(sizeof(buffer_t));
    memset(buffer, 0, sizeof(*buffer));
    return buffer;
}

void destroy_buffer(buffer_t* buffer) {
    if (buffer != NULL) {
        arrfree(buffer->buffer);
        free(buffer);
    }
}

int bputc(int c, buffer_t* buffer) {
    arrpush(buffer->buffer, (unsigned char)c);
    return (int)((unsigned char)c);
}

int bgetc(buffer_t* buffer) {
    if (buffer->read_index < arrlen(buffer->buffer)) {
        return buffer->buffer[buffer->read_index++];
    }
    return END_OF_BUFFER;
}

int bprintf(buffer_t* buffer, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int ret = vfctprintf((void*)bputc, buffer, fmt, ap);
    va_end(ap);
    return ret;
}
