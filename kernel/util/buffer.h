#pragma once

typedef struct buffer {
    char* buffer;
    int read_index;
} buffer_t;

/**
 * Create a new in-memory buffer
 */
buffer_t* create_buffer();

/**
 * Destroy an allocator buffer
 * @param buffer
 */
void destroy_buffer(buffer_t* buffer);

/**
 * Represents the buffer got no more chars in it
 */
#define END_OF_BUFFER (-1)

/**
 * Write a character to the buffer, always returns 0
 *
 * @param c         [IN] The character to write
 * @param buffer    [IN] The buffer
 */
int bputc(int c, buffer_t* buffer);

/**
 * Get a character from the buffer, return END_OF_BUFFER if
 * we got no more bytes in the buffer
 *
 * @param buffer    [IN] The buffer
 */
int bgetc(buffer_t* buffer);

/**
 * Print a formatted string to the buffer
 */
int bprintf(buffer_t* buffer, const char* fmt, ...);
