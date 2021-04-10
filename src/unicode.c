// SPDX-License-Identifier: GPL-2.0-only
/*
 * unicode.c
 *
 * Copyright (C) 2021  Imran Haider
 */

#include <unicode.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>

void unicode_utf32_string_free(utf32_t *restrict *restrict str, size_t count)
{
	size_t i;

	for (i=0; i<count; ++i)
		free(str[i]);
}

void unicode_utf8_string_free(char *restrict *restrict str, size_t count)
{
	size_t i;

	for (i=0; i<count; ++i)
		free(str[i]);
}

int unicode_write_utf8_file(int fd, const char *filename, const utf32_t *in_str, size_t in_size)
{
	int out_fd, rc = 0;
	size_t utf8_size = 0;
	char *utf8_buf;

	rc = unicode_write_utf8_string(in_str, in_size, &utf8_buf, &utf8_size);
	if (__builtin_expect(rc != 0, 0)) {
		goto exit1;
	}

	/* open the output file */
	out_fd = openat(fd, filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (__builtin_expect(out_fd == -1, 0)) {
		rc = errno;
		fprintf(stderr, "cannot open '%s' for writing. error %d\n", filename, rc);
		goto exit2;
	}

	/* write utf-8 data into file */
	if (__builtin_expect(write(out_fd, utf8_buf, utf8_size) == -1, 0)) {
		rc = errno;
		fprintf(stderr, "cannot write to '%s'. error %d\n", filename, rc);
	}

	/* clean up */
	close(out_fd);
exit2:
	free(utf8_buf);
exit1:
	return rc;
}

int unicode_write_utf8_string(
		const utf32_t *restrict in_str, size_t in_size, char **restrict out_str,
		size_t *restrict out_size)
{
	int rc = 0;
	size_t i;
	char *p;

	/* In the worst case, a utf-8 encoded character can take 50% more space
	 * compared to its equivalent utf-32 encoded character. For this reason
	 * we are allocating at least 150% to store the utf-8 encoded string.
	 * To guard against rounding errors, we will actually allocate memory
	 * that is 160% the size of the utf-32 string.
	 */
	const size_t utf8_buf_size = sizeof(utf32_t) * (in_size * 1.6) + *out_size;

	*out_str = malloc(utf8_buf_size);
	if (__builtin_expect(*out_str == 0, 0)) {
		rc = ENOMEM;
		fprintf(stderr, "not enough memory to allocate %ld bytes\n", utf8_buf_size);
		goto exit1;
	}

	p = *out_str;
	for (i=0; i < in_size; ++i) {
		unicode_write_utf8_char(&p, in_str[i]);
	}

	*out_size = p - *out_str;
exit1:
	return rc;
}

int unicode_write_utf8_char(char **restrict s, utf32_t ch)
{
	uint8_t *p = (uint8_t*) *s;

	if (__builtin_expect(ch < 0, 0)) {
		return -1;
	}
	else if (ch < 128) {
		p[0] = ch;
		++*s;
	}
	else {
#ifdef __BMI2__
		uint32_t i, cnt = 6 - (__builtin_ia32_lzcnt_u32(ch) - 1) / 5;
#else
		int i, cnt = (ch != 0)? 6 - (__builtin_clz(ch) - 1) / 5 : 0;
#endif
		p[0] = 255 << (8 - cnt) | ch >> 6 * (cnt - 1);

		for (i=1; i < cnt; ++i) {
			p[i] = 128 | (ch >> 6 * (cnt - i - 1) & 63);
		}

		*s += cnt;
	}

	return 0;
}

int unicode_read_utf8_file(int fd, const char *filename, utf32_t **out_str, size_t *out_size)
{
	int in_fd, rc;

	in_fd = openat(fd, filename, O_RDONLY);
	if (__builtin_expect(in_fd == -1, 0)) {
		rc = errno;
		fprintf(stderr, "cannot open '%s' for reading. error %d\n", filename, rc);
		goto exit1;
	}

	size_t in_size = lseek(in_fd, 0, SEEK_END);
	if (__builtin_expect(in_size == -1, 0)) {
		rc = errno;
		fprintf(stderr, "cannot check the file size of '%s'. error %d\n", filename, rc);
		goto exit2;
	}

	void *in_data = mmap(0, in_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if (__builtin_expect(in_data == MAP_FAILED, 0)) {
		rc = errno;
		fprintf(stderr, "cannot memory map '%s' for reading. error %d\n", filename, rc);
		goto exit2;
	}

	rc = unicode_read_utf8_string(in_data, in_size, out_str, out_size);
	munmap(in_data, in_size);
exit2:
	close(in_fd);
exit1:
	return rc;
}

int unicode_read_utf8_string(
		const char *in_str, size_t in_size,
		utf32_t *restrict *restrict out_str, size_t *restrict out_size)
{
	int rc = 0;
	size_t i;
	const char *p, *q;

	/* In the worst case, all the characters in the utf-8 string are
	 * single-byte characters, and so we would need to allocate one
	 * int per character.
	 */
	const size_t buf_size = sizeof(utf32_t) * in_size + *out_size;

	*out_str = malloc(buf_size);
	if (__builtin_expect(*out_str == 0, 0)) {
		rc = ENOMEM;
		fprintf(stderr, "not enough memory to allocate %ld bytes\n", buf_size);
		goto exit1;
	}

	for (p = in_str, q = p + in_size, i = 0; p < q; ++i) {
		(*out_str)[i] = unicode_read_utf8_char(&p);
	}

	*out_size = i;
exit1:
	return rc;
}

int32_t unicode_read_utf8_char(const char **restrict s)
{
	const uint8_t *p = (const uint8_t *) *s;
	utf32_t code = 0;

#ifdef __BMI2__
	uint16_t ch1 = p[0];
	uint16_t i, cnt = __builtin_ia32_lzcnt_u16(~ch1 << 8);
#else
	uint32_t ch1 = p[0];
	uint32_t lshift_invert = ~ch1 << 24;
	int i, cnt = (lshift_invert)? __builtin_clz(lshift_invert) : 0;
#endif

	if (cnt == 0) {
		++*s;
		code = ch1 & 127;
	}
	else if (cnt < 6) {
		for (i=1; i < cnt; ++i)
			if ((p[i] & 192) == 128)
				code = code << 6 | (p[i] & 63);
			else
				goto fail;

		*s += cnt;
		code = (ch1 & (255 >> cnt)) << (cnt - 1) * 6 | code;
	}
	else {
fail:
		code = -1;
	}

	return code;
}

int unicode_read_ascii_string(
		const char *in_str, size_t in_size,
		utf32_t *restrict *restrict out_str, size_t *restrict out_size)
{
	int rc = 0;
	size_t i;
	const char *p;
	const size_t buf_size = sizeof(utf32_t) * in_size + *out_size;

	*out_str = malloc(buf_size);
	*out_size = in_size;

	if (__builtin_expect(*out_str == 0, 0)) {
		rc = ENOMEM;
		fprintf(stderr, "not enough memory to allocate %ld bytes\n", buf_size);
		goto exit1;
	}

	for (i=0, p=in_str; i < in_size; ++i) {
		(*out_str)[i] = unicode_read_ascii_char(&p);
	}

exit1:
	return rc;
}

utf32_t unicode_read_ascii_char(const char **restrict s)
{
	utf32_t code = **s;
	++*s;
	return code;
}

int unicode_find(
		const utf32_t *restrict hay, const utf32_t *restrict needle,
		size_t hay_size, size_t needle_size)
{
	size_t i, hay_minus_needle;

	if (__builtin_expect(hay_size == 0 || needle_size == 0, 0))
		return -1;

	hay_minus_needle = hay_size - needle_size;

	/* search for the first needle character in the hay with the expectation
	 * that it will not match (which is the most common case). once we find
	 * the first matching character, we check the remaining characters in the
	 * needle with the expectation that it will match.
	 */
	for (i=0; i<=hay_minus_needle; ++i) {
		if (hay[i] != *needle) {
			continue;
		}

		if (__builtin_expect(unicode_compare_likely_equal(hay + i, needle, needle_size) == 0, 1))
			return i;
	}

	return -1;
}

utf32_t unicode_compare_likely_equal(const utf32_t *s1, const utf32_t *s2, size_t size)
{
	size_t i;
	utf32_t diff = 0;

	for (i=0; i<size; ++i) {
		diff = s1[i] - s2[i];
		if (__builtin_expect(diff, 0))
			break;
	}

	return diff;
}

utf32_t unicode_compare_likely_different(const utf32_t *s1, const utf32_t *s2, size_t size)
{
	size_t i;
	utf32_t diff = 0;

	for (i=0; i<size; ++i) {
		diff = s1[i] - s2[i];
		if (__builtin_expect(diff, 1))
			break;
	}

	return diff;
}

