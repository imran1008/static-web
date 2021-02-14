// SPDX-License-Identifier: GPL-2.0-only
/*
 * html_lexer.c
 *
 * Copyright (C) 2021  Imran Haider
 */

#include <html_lexer.h>
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

struct html_lexer_t {
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

static int read_token(struct html_lexer_t *restrict lexer);
static int read_token_word(struct html_lexer_t *restrict lexer);
static int read_token_whitespace(struct html_lexer_t *restrict lexer);
static int read_token_char(struct html_lexer_t *restrict lexer, char ch, uint8_t token_id);
static int read_token_keyword(
		struct html_lexer_t *restrict lexer, int keyword_id, uint8_t token_id);
static int read_token_string(struct html_lexer_t *restrict lexer);
static int read_token_cdata(
		struct html_lexer_t *restrict lexer, size_t keyword_begin, size_t keyword_end,
		uint8_t token_id);
static int read_token_first_text(struct html_lexer_t *restrict lexer);
static int read_token_text(struct html_lexer_t *restrict lexer);
static void add_token(
		struct html_lexer_t *restrict lexer, uint8_t id,
		const int32_t *begin, const int32_t *end);

int html_lex(
		const int32_t *restrict in_data, size_t in_size, struct html_tokens_t *restrict tokens)
{
	struct html_lexer_t lexer = {0};
	int rc = 0;

	lexer.tokens = tokens;

#define REGISTER_KEYWORD(id,str) unicode_read_ascii_string(str, sizeof(str)-1, lexer.keyword_data+id, lexer.keyword_size+id)
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

	lexer.current = in_data;
	lexer.end = in_data + in_size;

	/* process first token */
	int processed = read_token(&lexer) || read_token_first_text(&lexer);
	if (!processed) {
		lexer.exception_pending = 1;
		lexer.exception_msg = "Unrecognized token";
		lexer.exception_location = lexer.current;
	}

	/* process all remaining tokens */
	if (lexer.exception_pending == 0) {
		while (lexer.current < lexer.end) {
			processed = read_token(&lexer) || read_token_text(&lexer);

			if (processed) {
				if (__builtin_expect(lexer.exception_pending, 0))
					break;
			}
			else {
				lexer.exception_pending = 1;
				lexer.exception_msg = "Unrecognized token";
				lexer.exception_location = lexer.current;
				break;
			}
		}
	}

	if (__builtin_expect(lexer.exception_pending, 0)) {
		const int *p;
		int column_num = 0;
		int line_num = 0;

		for (p = in_data; p < lexer.exception_location; ++p) {
			if (*p != '\n') {
				++column_num;	
			}
			else {
				++line_num;
				column_num = 0;
			}
		}

		fprintf(stderr, "html_lex: %s on line %d, column %d\n",
				lexer.exception_msg, line_num, column_num);
		rc = -1;
	}

	unicode_string_free(lexer.keyword_data, KEYWORD_END);
	return rc;
}

static int read_token(struct html_lexer_t *restrict lexer)
{
	return 
		   read_token_cdata(lexer, KEYWORD_COMMENT_START, KEYWORD_COMMENT_END, HTML_TOKEN_COMMENT)
		|| read_token_cdata(lexer, KEYWORD_SCRIPT_START, KEYWORD_SCRIPT_END, HTML_TOKEN_SCRIPT)
		|| read_token_cdata(lexer, KEYWORD_STYLE_START, KEYWORD_STYLE_END, HTML_TOKEN_STYLE)
		|| read_token_string(lexer)
		|| read_token_char(lexer, '>',  HTML_TOKEN_GREATERTHAN)
		|| read_token_char(lexer, '<',  HTML_TOKEN_LESSTHAN)
		|| read_token_char(lexer, '\'', HTML_TOKEN_SINGLEQUOTE)
		|| read_token_char(lexer, '"',  HTML_TOKEN_DOUBLEQUOTE)
		|| read_token_char(lexer, '&',  HTML_TOKEN_AMPERSAND)
		|| read_token_char(lexer, '!',  HTML_TOKEN_EXCLAMATIONMARK)
		|| read_token_char(lexer, '=',  HTML_TOKEN_EQUAL)
		|| read_token_char(lexer, '-',  HTML_TOKEN_HYPHEN)
		|| read_token_char(lexer, ':',  HTML_TOKEN_COLON)
		|| read_token_char(lexer, '{',  HTML_TOKEN_OPENBRACE)
		|| read_token_char(lexer, '}',  HTML_TOKEN_CLOSEBRACE)
		|| read_token_char(lexer, '(',  HTML_TOKEN_OPENPAREN)
		|| read_token_char(lexer, ')',  HTML_TOKEN_CLOSEPAREN)
		|| read_token_char(lexer, ';',  HTML_TOKEN_SEMICOLON)
		|| read_token_char(lexer, '*',  HTML_TOKEN_ASTERISK)
		|| read_token_char(lexer, '#',  HTML_TOKEN_HASH)
		|| read_token_char(lexer, ',',  HTML_TOKEN_COMMA)
		|| read_token_keyword(lexer, KEYWORD_HTML, HTML_TOKEN_HTML)
		|| read_token_keyword(lexer, KEYWORD_DATA, HTML_TOKEN_DATA)
		|| read_token_word(lexer)
		|| read_token_whitespace(lexer);
}

static int read_token_word(struct html_lexer_t *restrict lexer)
{
	const int *restrict p = lexer->current;

	if (__builtin_expect((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && *p != '_', 0))
		return 0;

	++p;
	while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
			*p == '_')
		++p;

	add_token(lexer, HTML_TOKEN_WORD, lexer->current, p);
	lexer->current = p;
	return 1;
}

static int read_token_whitespace(struct html_lexer_t *restrict lexer)
{
	const int *restrict p = lexer->current;

	while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
		++p;

	if (p - lexer->current) {
		add_token(lexer, HTML_TOKEN_WHITESPACE, lexer->current, p);
		lexer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_char(struct html_lexer_t *restrict lexer, char ch, uint8_t token_id)
{
	const int *restrict p = lexer->current;

	if (*lexer->current == ch) {
		++p;
		add_token(lexer, token_id, lexer->current, p);
		lexer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_keyword(
		struct html_lexer_t *restrict lexer, int keyword_id, uint8_t token_id)
{
	size_t size = lexer->keyword_size[keyword_id];
	const int *restrict p = lexer->current;

	if (unicode_compare_likely_equal(p, lexer->keyword_data[keyword_id], size) == 0) {
		p += size;
		add_token(lexer, token_id, lexer->current, p);
		lexer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_string(struct html_lexer_t *restrict lexer)
{
	const int *restrict p = lexer->current;

	if (__builtin_expect(*p != '"', 0))
		return 0;

	++p;
	while (p < lexer->end) {
		if (*p == '\\') {
			p += 2;
		}
		else if (*p == '"') {
			add_token(lexer, HTML_TOKEN_STRING, lexer->current+1, p);
			lexer->current = p;
			return 1;
		}
		else {
			++p;
		}
	}

	lexer->exception_pending = 1;
	lexer->exception_msg = "Unterminated string literal";
	lexer->exception_location = lexer->current;
	return 1;
}

static int read_token_cdata(
		struct html_lexer_t *restrict lexer, size_t keyword_begin, size_t keyword_end,
		uint8_t token_id)
{
	const int *restrict p = lexer->current;
	const int *keyword_data = lexer->keyword_data[keyword_begin];
	size_t keyword_size = lexer->keyword_size[keyword_begin];

	if (unicode_compare_likely_equal(p, keyword_data, keyword_size) == 0) {
		p += keyword_size;

		keyword_data = lexer->keyword_data[keyword_end];
		keyword_size = lexer->keyword_size[keyword_end];

		int res = unicode_find(p, keyword_data, lexer->end - p, keyword_size);

		if (res > -1) {
			p += res;
			add_token(lexer, token_id, lexer->current, p);
			lexer->current = p;
			return 1;
		}
	}

	return 0;
}

static int read_token_first_text(struct html_lexer_t *restrict lexer)
{
	const int *restrict p = lexer->current;

	while (*p != '<')
		++p;

	if (p - lexer->current) {
		add_token(lexer, HTML_TOKEN_TEXT, lexer->current, p);
		lexer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_text(struct html_lexer_t *restrict lexer)
{
	const int *restrict p = lexer->current - 1;

	if (*p == '>') {
		++p;

		while (*p != '<')
			++p;

		if (p - lexer->current) {
			add_token(lexer, HTML_TOKEN_TEXT, lexer->current, p);
			lexer->current = p;
			return 1;
		}
	}

	return 0;
}

static void add_token(
		struct html_lexer_t *restrict lexer, uint8_t id,
		const int32_t *begin, const int32_t *end)
{
	struct html_tokens_t *restrict tokens = lexer->tokens;
	size_t i = tokens->count;

	if (i < tokens->capacity) {
		tokens->begin[i] = begin;
		tokens->end[i] = end;
		tokens->id[i] = id;

		tokens->count = i+1;
	}
	else {
		lexer->exception_pending = 1;
		lexer->exception_msg = "Not enough space for tokens";
		lexer->exception_location = begin;
	}
}


