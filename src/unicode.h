/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * unicode.h
 *
 * Copyright (C) 2021  Imran Haider
 */

#ifndef UNICODE_H
#define UNICODE_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t utf32_t;

/* Release memory held by the utf32 strings
 */
void unicode_utf32_string_free(utf32_t *restrict *restrict str, size_t count);

/* Release memory held by the utf8 unicode strings
 */
void unicode_utf8_string_free(char *restrict *restrict str, size_t count);

/* Encode a utf-32 string into a utf-8 string and write it to the specified
 * file. 'filename' is relative with respect to the directory referenced
 * by 'fd'.
 */
int unicode_write_utf8_file(int fd, const char *filename, const utf32_t *in_str, size_t in_size);

/* Encodes the unicode string 'in_str' and writes the utf-8 string into
 * 'out_str'. The function estimates the memory allocation size based on
 * the worst case scenario and it also pads the allocation request size
 * with the initial value of 'out_size'. This allows the caller to guarantee
 * an additional padding at the end of the string.
 *
 * The number of bytes written to the output is recorded in the output
 * parameter 'out_size'. The caller is expected to call
 * unicode_utf8_string_free to release the memory allocated in this function.
 */
int unicode_write_utf8_string(
		const utf32_t *restrict in_str, size_t in_size, char **restrict out_str,
		size_t *restrict out_size);

/* Encodes the unicode character 'ch' as UTF-8 and write the bytes into
 * the string '*s'. The 's' pointer is incremented to the next character
 * position.
 */
int unicode_write_utf8_char(char **restrict s, utf32_t ch);

/* Decode a utf-8 file into a utf-32 string. 'filename' is relative with
 * respect to the directory referenced by 'fd'. The function estimates the
 * memory allocation size based on the worst case scenario and it also pads
 * the allocation request size with the initial value of 'out_str.size'. This
 * allows the caller to guarantee an additional padding at the end of the
 * string.
 *
 * The caller is expected to call unicode_utf32_string_free to release the
 * memory allocated in this function.
 */
int unicode_read_utf8_file(int fd, const char *filename, utf32_t **out_str, size_t *out_size);

/* Decodes the utf-8 string 'in_str' and writes the utf-32 string into
 * 'out_str'. The function estimates the memory allocation size based on
 * the worst case scenario and it also pads the allocation request size
 * with the initial value of 'out_str.size'. This allows the caller to guarantee
 * an additional padding at the end of the string.
 *
 * The number of characters written is recorded in the output
 * parameter 'out_str.size'. The caller is expected to call
 * unicode_utf32_string_free to release the memory allocated in this function.
 */
int unicode_read_utf8_string(
		const char *in_str, size_t in_size, utf32_t *restrict *restrict out_str,
		size_t *restrict out_size);

/* Decodes the UTF-8 character stored in the string '*s' and returns it
 * as a unicode character. The 's' pointer is incremented to the next
 * character position.
 */
int unicode_read_utf8_char(const char **restrict s);

/* Reads an ASCII string 'in_str' and writes the utf-32 equivalent into
 * 'out_str'. The function performs a memory allocation that is padded
 * with the inital value of 'out_str.size'. This allows teh caller to guarantee
 * an additional padding at the end of the string.
 *
 * The number of characters written is recoreded in the output parameter
 * 'out_str.size'. The caller is epected to call unicode_utf32_string_free to
 * release the memory allocated in this function.
 */
int unicode_read_ascii_string(
		const char *in_str, size_t in_size,
		utf32_t *restrict *restrict out_str, size_t *restrict out_size);

/* Return the ASCII character as an unicode character. The 's' pointer
 * is incremented to the next character position
 */
utf32_t unicode_read_ascii_char(const char **restrict s);

/* Find the first 'needle' in the 'hay' and return its index. If the string
 * is not found, return -1.
 */
int unicode_find(
		const utf32_t *restrict hay, const utf32_t *restrict needle,
		size_t hay_size, size_t needle_size);

/* Compares 's1' and 's2' for upto 'size' characters'. The function returns the
 * difference of the first divergent character of 's2' from the same indexed
 * character of 's1'. If the strings are equal, the return value is 0.
 *
 * Use this function if you suspect the two strings are equal.
 */
utf32_t unicode_compare_likely_equal(const utf32_t *s1, const utf32_t *s2, size_t size);

/* Compares 's1' and 's2' for upto 'size' characters'. The function returns the
 * difference of the first divergent character of 's2' from the same indexed
 * character of 's1'. If the strings are equal, the return value is 0.
 *
 * Use this function if you suspect the two strings are different.
 */
utf32_t unicode_compare_likely_different(const utf32_t *s1, const utf32_t *s2, size_t size);

#endif

