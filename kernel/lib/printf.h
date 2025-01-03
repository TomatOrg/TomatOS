/**
 * @author (c) Eyal Rozenberg <eyalroz1@gmx.com>
 *             2021-2023, Haifa, Palestine/Israel
 * @author (c) Marco Paland (info@paland.com)
 *             2014-2019, PALANDesign Hannover, Germany
 *
 * @note Others have made smaller contributions to this file: see the
 * contributors page at https://github.com/eyalroz/printf/graphs/contributors
 * or ask one of the authors.
 *
 * @brief Small stand-alone implementation of the printf family of functions
 * (`(v)printf`, `(v)s(n)printf` etc., geared towards use on embedded systems
 * with a very limited resources.
 *
 * @note the implementations are thread-safe; re-entrant; use no functions from
 * the standard library; and do not dynamically allocate any memory.
 *
 * @license The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <stdarg.h>
#include <stddef.h>

/**
 * Prints/send a single character to some opaque output entity
 *
 * @note This function is not implemented by the library, only declared; you
 * must provide an implementation if you wish to use the @ref printf / @ref
 * vprintf function (and possibly for linking against the library, if your
 * toolchain does not support discarding unused functions)
 *
 * @note The output could be as simple as a wrapper for the `write()` system
 * call on a Unix-like * system, or even libc's @ref putchar , for replicating
 * actual functionality of libc's @ref printf * function; but on an embedded
 * system it may involve interaction with a special output device, like a UART,
 * etc.
 *
 * @note in libc's @ref putchar, the parameter type is an int; this was intended
 * to support the representation of either a proper character or EOF in a
 * variable - but this is really not meaningful to pass into @ref putchar and is
 * discouraged today. See further discussion in:
 * @link https://stackoverflow.com/q/17452847/1593077
 *
 * @param c the single character to print
 */
void kputchar(char c);

/**
 * An implementation of the C standard's printf/vprintf
 *
 * @note you must implement a @ref putchar_ function for using this function -
 * it invokes @ref putchar_ * rather than directly performing any I/O (which
 * insulates it from any dependence on the operating system * and external
 * libraries).
 *
 * @param format A string specifying the format of the output, with %-marked
 *     specifiers of how to interpret additional arguments.
 * @param arg Additional arguments to the function, one for each %-specifier in
 *     @p format
 * @return The number of characters written into @p s, not counting the
 *     terminating null character
 */
int kprintf(const char* format, ...);
int kvprintf(const char* format, va_list arg);


/**
 * An implementation of the C standard's sprintf/vsprintf
 *
 * @note For security considerations (the potential for exceeding the buffer
 * bounds), please consider using the size-constrained variant, @ref snprintf /
 * @ref vsnprintf, instead.
 *
 * @param s An array in which to store the formatted string. It must be large
 *     enough to fit the formatted output!
 * @param format A string specifying the format of the output, with %-marked
 *     specifiers of how to interpret additional arguments
 * @param arg Additional arguments to the function, one for each specifier in
 *     @p format
 * @return The number of characters written into @p s, not counting the
 *     terminating null character
 */
int ksprintf(char* s, const char* format, ...);
int kvsprintf(char* s, const char* format, va_list arg);


/**
 * An implementation of the C standard's snprintf/vsnprintf
 *
 * @param s An array in which to store the formatted string. It must be large
 *     enough to fit either the entire formatted output, or at least @p n
 *     characters. Alternatively, it can be NULL, in which case nothing will
 *     be printed, and only the number of characters which _could_ have been
 *     printed is tallied and returned.
 * @param n The maximum number of characters to write to the array, including
 *     a terminating null character
 * @param format A string specifying the format of the output, with %-marked
 *     specifiers of how to interpret additional arguments.
 * @param arg Additional arguments to the function, one for each specifier in
 *     @p format
 * @return The number of characters that COULD have been written into @p s, not
 *     counting the terminating null character. A value equal or larger than
 *     @p n indicates truncation. Only when the returned value is non-negative
 *     and less than @p n, the null-terminated string has been fully and
 *     successfully printed.
 */
int ksnprintf(char* s, size_t count, const char* format, ...);
int kvsnprintf(char* s, size_t count, const char* format, va_list arg);

/**
 * printf/vprintf with user-specified output function
 *
 * An alternative to @ref printf_, in which the output function is specified
 * dynamically (rather than @ref putchar_ being used)
 *
 * @param out An output function which takes one character and a type-erased
 *     additional parameters
 * @param extra_arg The type-erased argument to pass to the output function @p
 *     out with each call
 * @param format A string specifying the format of the output, with %-marked
 *     specifiers of how to interpret additional arguments.
 * @param arg Additional arguments to the function, one for each specifier in
 *     @p format
 * @return The number of characters for which the output f unction was invoked,
 *     not counting the terminating null character
 *
 */
int kfctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, ...);
int kvfctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, va_list arg);
