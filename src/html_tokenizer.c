// SPDX-License-Identifier: GPL-2.0-only
/*
 * html_tokenizer.c
 *
 * Copyright (C) 2021  Imran Haider
 */

#include <html_tokenizer.h>
#include <html_parser.h>
#include <unicode.h>

#include <stdio.h>
#include <string.h>

enum {
	KEYWORD_HTML,
	KEYWORD_DATA,
	KEYWORD_INCLUDE,
	KEYWORD_SCRIPT_START,
	KEYWORD_SCRIPT_END,
	KEYWORD_STYLE_START,
	KEYWORD_STYLE_END,
	KEYWORD_COMMENT_START,
	KEYWORD_COMMENT_END,
	KEYWORD_TEXT_START,
	KEYWORD_TEXT_END,
	KEYWORD_END
};

struct html_tokenizer_t {
	const int32_t *restrict current;
	const int32_t *restrict end;

	/* tokens */
	struct html_tokens_t *restrict tokens;
	
	/* keywords */
	int32_t *restrict keyword_data[KEYWORD_END];
	size_t keyword_size[KEYWORD_END];

	/* parsing error handling */
	const char *restrict exception_msg;
	const int32_t *restrict exception_location;

	/* flags */
	unsigned int exception_pending :1;
};

static int read_token(struct html_tokenizer_t *restrict tokenizer);
static int read_token_word(struct html_tokenizer_t *restrict tokenizer);
static int read_token_whitespace(struct html_tokenizer_t *restrict tokenizer);
static int read_token_char(struct html_tokenizer_t *restrict tokenizer, char ch, uint8_t token_id);
static int read_token_keyword(
		struct html_tokenizer_t *restrict tokenizer, int keyword_id, uint8_t token_id);
static int read_token_string(struct html_tokenizer_t *restrict tokenizer);
static int read_token_cdata(
		struct html_tokenizer_t *restrict tokenizer, size_t keyword_begin, size_t keyword_end,
		uint8_t token_id);
static int read_token_first_text(struct html_tokenizer_t *restrict tokenizer);
static int read_token_text(struct html_tokenizer_t *restrict tokenizer);
static void add_token(
		struct html_tokenizer_t *restrict tokenizer, uint8_t id,
		const int32_t *begin, const int32_t *end);

int html_tokenize(
		const int32_t *restrict in_data, size_t in_size, struct html_tokens_t *restrict tokens)
{
	struct html_tokenizer_t tokenizer = {0};
	int rc = 0;

	tokenizer.tokens = tokens;

#define REGISTER_KEYWORD(id,str) unicode_read_ascii_string(str, sizeof(str)-1, tokenizer.keyword_data+id, tokenizer.keyword_size+id)
	REGISTER_KEYWORD(KEYWORD_HTML,          "html");
	REGISTER_KEYWORD(KEYWORD_DATA,          "data");
	REGISTER_KEYWORD(KEYWORD_INCLUDE,       "include");
	REGISTER_KEYWORD(KEYWORD_SCRIPT_START,  "<script");
	REGISTER_KEYWORD(KEYWORD_SCRIPT_END,    "</script>");
	REGISTER_KEYWORD(KEYWORD_STYLE_START,   "<style");
	REGISTER_KEYWORD(KEYWORD_STYLE_END,     "</style>");
	REGISTER_KEYWORD(KEYWORD_COMMENT_START, "<!--");
	REGISTER_KEYWORD(KEYWORD_COMMENT_END,   "-->");
	REGISTER_KEYWORD(KEYWORD_TEXT_START,    ">");
	REGISTER_KEYWORD(KEYWORD_TEXT_END,      "<");

	tokenizer.current = in_data;
	tokenizer.end = in_data + in_size;

	/* process first token */
	int processed = read_token(&tokenizer) || read_token_first_text(&tokenizer);
	if (!processed) {
		tokenizer.exception_pending = 1;
		tokenizer.exception_msg = "Unrecognized token";
		tokenizer.exception_location = tokenizer.current;
	}

	/* process all remaining tokens */
	if (tokenizer.exception_pending == 0) {
		while (tokenizer.current < tokenizer.end) {
			processed = read_token(&tokenizer) || read_token_text(&tokenizer);

			if (processed) {
				if (__builtin_expect(tokenizer.exception_pending, 0))
					break;
			}
			else {
				tokenizer.exception_pending = 1;
				tokenizer.exception_msg = "Unrecognized token";
				tokenizer.exception_location = tokenizer.current;
				break;
			}
		}
	}

	if (__builtin_expect(tokenizer.exception_pending, 0)) {
		const int *p;
		int column_num = 0;
		int line_num = 0;

		for (p = in_data; p < tokenizer.exception_location; ++p) {
			if (*p != '\n') {
				++column_num;	
			}
			else {
				++line_num;
				column_num = 0;
			}
		}

		fprintf(stderr, "html_tokenizer: %s on line %d, column %d\n",
				tokenizer.exception_msg, line_num, column_num);
		rc = -1;
	}

	unicode_string_free(tokenizer.keyword_data, KEYWORD_END);
	return rc;
}

static int read_token(struct html_tokenizer_t *restrict tokenizer)
{
	return 
		   read_token_cdata(tokenizer, KEYWORD_COMMENT_START, KEYWORD_COMMENT_END, HTML_TOKEN_COMMENT)
		|| read_token_cdata(tokenizer, KEYWORD_SCRIPT_START, KEYWORD_SCRIPT_END, HTML_TOKEN_SCRIPT)
		|| read_token_cdata(tokenizer, KEYWORD_STYLE_START, KEYWORD_STYLE_END, HTML_TOKEN_STYLE)
		|| read_token_string(tokenizer)
		|| read_token_char(tokenizer, '>',  HTML_TOKEN_GREATERTHAN)
		|| read_token_char(tokenizer, '<',  HTML_TOKEN_LESSTHAN)
		|| read_token_char(tokenizer, '\'', HTML_TOKEN_SINGLEQUOTE)
		|| read_token_char(tokenizer, '"',  HTML_TOKEN_DOUBLEQUOTE)
		|| read_token_char(tokenizer, '&',  HTML_TOKEN_AMPERSAND)
		|| read_token_char(tokenizer, '!',  HTML_TOKEN_EXCLAMATIONMARK)
		|| read_token_char(tokenizer, '=',  HTML_TOKEN_EQUAL)
		|| read_token_char(tokenizer, '-',  HTML_TOKEN_HYPHEN)
		|| read_token_char(tokenizer, ':',  HTML_TOKEN_COLON)
		|| read_token_char(tokenizer, '{',  HTML_TOKEN_OPENBRACE)
		|| read_token_char(tokenizer, '}',  HTML_TOKEN_CLOSEBRACE)
		|| read_token_char(tokenizer, '(',  HTML_TOKEN_OPENPAREN)
		|| read_token_char(tokenizer, ')',  HTML_TOKEN_CLOSEPAREN)
		|| read_token_char(tokenizer, ';',  HTML_TOKEN_SEMICOLON)
		|| read_token_char(tokenizer, '*',  HTML_TOKEN_ASTERISK)
		|| read_token_char(tokenizer, '#',  HTML_TOKEN_HASH)
		|| read_token_char(tokenizer, ',',  HTML_TOKEN_COMMA)
		|| read_token_keyword(tokenizer, KEYWORD_HTML, HTML_TOKEN_HTML)
		|| read_token_keyword(tokenizer, KEYWORD_DATA, HTML_TOKEN_DATA)
		|| read_token_word(tokenizer)
		|| read_token_whitespace(tokenizer);
}

static int read_token_word(struct html_tokenizer_t *restrict tokenizer)
{
	const int *restrict p = tokenizer->current;

	if (__builtin_expect((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && *p != '_', 0))
		return 0;

	++p;
	while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
			*p == '_')
		++p;

	add_token(tokenizer, HTML_TOKEN_WORD, tokenizer->current, p);
	tokenizer->current = p;
	return 1;
}

static int read_token_whitespace(struct html_tokenizer_t *restrict tokenizer)
{
	const int *restrict p = tokenizer->current;

	while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
		++p;

	if (p - tokenizer->current) {
		add_token(tokenizer, HTML_TOKEN_WHITESPACE, tokenizer->current, p);
		tokenizer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_char(struct html_tokenizer_t *restrict tokenizer, char ch, uint8_t token_id)
{
	const int *restrict p = tokenizer->current;

	if (*tokenizer->current == ch) {
		++p;
		add_token(tokenizer, token_id, tokenizer->current, p);
		tokenizer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_keyword(
		struct html_tokenizer_t *restrict tokenizer, int keyword_id, uint8_t token_id)
{
	size_t size = tokenizer->keyword_size[keyword_id];
	const int *restrict p = tokenizer->current;

	if (unicode_compare_likely_equal(p, tokenizer->keyword_data[keyword_id], size) == 0) {
		p += size;
		add_token(tokenizer, token_id, tokenizer->current, p);
		tokenizer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_string(struct html_tokenizer_t *restrict tokenizer)
{
	const int *restrict p = tokenizer->current;

	if (__builtin_expect(*p != '"', 0))
		return 0;

	++p;
	while (p < tokenizer->end) {
		if (*p == '\\') {
			p += 2;
		}
		else if (*p == '"') {
			add_token(tokenizer, HTML_TOKEN_STRING, tokenizer->current+1, p);
			tokenizer->current = p;
			return 1;
		}
		else {
			++p;
		}
	}

	tokenizer->exception_pending = 1;
	tokenizer->exception_msg = "Unterminated string literal";
	tokenizer->exception_location = tokenizer->current;
	return 1;
}

static int read_token_cdata(
		struct html_tokenizer_t *restrict tokenizer, size_t keyword_begin, size_t keyword_end,
		uint8_t token_id)
{
	const int *restrict p = tokenizer->current;
	const int *keyword_data = tokenizer->keyword_data[keyword_begin];
	size_t keyword_size = tokenizer->keyword_size[keyword_begin];

	if (unicode_compare_likely_equal(p, keyword_data, keyword_size) == 0) {
		p += keyword_size;

		keyword_data = tokenizer->keyword_data[keyword_end];
		keyword_size = tokenizer->keyword_size[keyword_end];

		int res = unicode_find(p, keyword_data, tokenizer->end - p, keyword_size);

		if (res > -1) {
			p += res;
			add_token(tokenizer, token_id, tokenizer->current, p);
			tokenizer->current = p;
			return 1;
		}
	}

	return 0;
}

static int read_token_first_text(struct html_tokenizer_t *restrict tokenizer)
{
	const int *restrict p = tokenizer->current;

	while (*p != '<')
		++p;

	if (p - tokenizer->current) {
		add_token(tokenizer, HTML_TOKEN_TEXT, tokenizer->current, p);
		tokenizer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_text(struct html_tokenizer_t *restrict tokenizer)
{
	const int *restrict p = tokenizer->current - 1;

	if (*p == '>') {
		++p;

		while (*p != '<')
			++p;

		if (p - tokenizer->current) {
			add_token(tokenizer, HTML_TOKEN_TEXT, tokenizer->current, p);
			tokenizer->current = p;
			return 1;
		}
	}

	return 0;
}

static void add_token(
		struct html_tokenizer_t *restrict tokenizer, uint8_t id,
		const int32_t *begin, const int32_t *end)
{
	struct html_tokens_t *restrict tokens = tokenizer->tokens;
	size_t i = tokens->count;

	if (i < tokens->capacity) {
		tokens->begin[i] = begin;
		tokens->end[i] = end;
		tokens->id[i] = id;

		tokens->count = i+1;
	}
	else {
		tokenizer->exception_pending = 1;
		tokenizer->exception_msg = "Not enough space for tokens";
		tokenizer->exception_location = begin;
	}
}


