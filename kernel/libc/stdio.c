#include "stdio.h"
#include "util/stb_ds.h"

#include <mem/mem.h>

FILE* stdout = (FILE*) -1;
FILE* stderr = (FILE*) -2;

FILE* fcreate() {
    return malloc(sizeof(FILE));
}

void fclose(FILE* steam) {
    if (steam != NULL) {
        arrfree(steam->buffer);
        free(steam);
    }
}

int fputc(int c, FILE* stream) {
    if (stream == stdout || stream == stderr) {
        _putchar((char)c);
    } else {
        arrpush(stream->buffer, (unsigned char)c);
    }
    return (int)((unsigned char)c);
}

int fgetc(FILE* steam) {
    ASSERT(steam != NULL);
    if (steam->read_index < arrlen(steam->buffer)) {
        return steam->buffer[steam->read_index++];
    }
    return EOF;
}

int fprintf(FILE* steam, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int ret = vfctprintf((void*)fputc, steam, fmt, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE* steam, const char* fmt, va_list ap) {
    return vfctprintf((void*)fputc, steam, fmt, ap);
}
