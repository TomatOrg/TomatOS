#include "stdio.h"
#include "util/stb_ds.h"

#include <mem/mem.h>

FILE* stdout = (FILE*) -1;
FILE* stderr = (FILE*) -2;

FILE* fcreate() {
    return malloc(sizeof(FILE));
}

void fclose(FILE* stream) {
    if (stream != NULL) {
        arrfree(stream->buffer);
        free(stream);
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
        return (int)(unsigned char)steam->buffer[steam->read_index++];
    }
    return EOF;
}

int fseek(FILE* stream, long offset, int whence) {
    ASSERT(stream != NULL && stream != stdout && stream != stderr);

    switch (whence) {
        case SEEK_SET: {
            if (offset >= arrlen(stream->buffer)) return -1;
            if (offset < 0) return -1;
            stream->read_index = offset;
        } break;

        case SEEK_CUR: {
            long new_index = stream->read_index + offset;
            if (new_index >= arrlen(stream->buffer)) return -1;
            if (new_index < 0) return -1;
            stream->read_index = new_index;
        } break;

        case SEEK_END: {
            if (offset > arrlen(stream->buffer)) return -1;
            if (offset < 0) return -1;
            stream->read_index = arrlen(stream->buffer) - offset;
        } break;

        default: {
            ASSERT("Invalid whence");
            return -1;
        }
    }

    return 0;
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
