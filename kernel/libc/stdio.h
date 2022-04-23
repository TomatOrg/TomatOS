#pragma once

#include <stddef.h>
#include <stdarg.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MIR requires quite a bit of standard library stuff, so we are going to implement it in here
// so MIR can work normally without us needing to do custom porting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct FILE {
    char* buffer;
    int read_index;
} FILE;

/**
 * These are dummy, we are going to use NULL for
 * just printing to normal stdout
 */
extern FILE* stdout;
extern FILE* stderr;

/**
 * Create a new in-memory buffer
 */
FILE* fcreate();

// stubs that will always fail because no way we support these stuff
static inline FILE* fopen(const char* path, const char* perm) { return NULL; }
static inline FILE *popen(const char *command, const char *type) { return NULL; }
static inline int pclose(FILE *stream) { return 0; }

/**
 * Destroy the stream
 */
void fclose(FILE* stream);

/**
 * Represents the buffer got no more chars in it
 */
#define EOF (-1)

/**
 * writes the character c, cast to an unsigned char,
 * to stream.
 */
int fputc(int c, FILE* steam);

/**
 * reads the next character from stream and returns
 * it as an unsigned char cast to an int, or EOF on
 * end of file or error.
 */
int fgetc(FILE* steam);

/**
 * Print a formatted string to the steam
 */
int fprintf(FILE* steam, const char* fmt, ...);

int vfprintf(FILE* steam, const char* fmt, va_list ap);

