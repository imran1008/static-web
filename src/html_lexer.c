// SPDX-License-Identifier: GPL-2.0-only
/*
 * html_lexer.c
 *
 * Copyright (C) 2021  Imran Haider
 */

#include <html_lexer.h>
#include <html_parser.h>

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
	KEYWORD_END
};

typedef uint8_t keyword_id_t;

enum {
	CHAR_INFO_IDENTIFIER = 1 << 0,
	CHAR_INFO_NUMBER     = 1 << 1,
	CHAR_INFO_NOT_TEXT   = 1 << 2,
	CHAR_INFO_WHITESPACE = 1 << 3
};

struct html_lexer_t {
	const utf32_t *restrict current;
	const utf32_t *restrict end;

	/* tokens */
	struct html_tokens_t *restrict tokens;

	/* keywords */
	utf32_t *restrict keyword_data[KEYWORD_END];
	size_t keyword_size[KEYWORD_END];

	/* lookup table for common characters */
	uint8_t char_info[128];

	/* parsing error handling */
	const char *restrict exception_msg;
	const utf32_t *restrict exception_location;

	/* flags */
	unsigned int exception_pending :1;
};

static int read_token(struct html_lexer_t *restrict lexer);
static int read_token_identifier(struct html_lexer_t *restrict lexer);
static int read_token_text(struct html_lexer_t *restrict lexer);
static int read_token_whitespace(struct html_lexer_t *restrict lexer);
static int read_token_char(struct html_lexer_t *restrict lexer, char ch, html_token_id_t token_id);
static int read_token_keyword(
		struct html_lexer_t *restrict lexer, keyword_id_t keyword_id, html_token_id_t token_id);
static int read_token_string(struct html_lexer_t *restrict lexer);
static int read_token_cdata(
		struct html_lexer_t *restrict lexer, size_t keyword_begin, size_t keyword_end,
		html_token_id_t token_id);
static void add_token(
		struct html_lexer_t *restrict lexer, html_token_id_t id,
		const utf32_t *begin, const utf32_t *end);

static inline int char_type_check(struct html_lexer_t *restrict lexer, utf32_t ch, int flags)
{
	// TODO: make this bound check branchless
	if (ch > 127)
		return 0;

	return lexer->char_info[ch] & flags;
}

int html_lex(
		const utf32_t *restrict in_data, size_t in_size, struct html_tokens_t *restrict tokens)
{
	struct html_lexer_t lexer = {0};
	int processed, rc = 0, i;

	lexer.tokens = tokens;

	/* prepare character info table */
	memset(&lexer.char_info, 0, sizeof(lexer.char_info));
	lexer.char_info['\n'] = CHAR_INFO_WHITESPACE;
	lexer.char_info[' '] = CHAR_INFO_WHITESPACE;
	lexer.char_info['\r'] = CHAR_INFO_WHITESPACE;
	lexer.char_info['\t'] = CHAR_INFO_WHITESPACE;

	/* HTML special characters */
	lexer.char_info['<'] = CHAR_INFO_NOT_TEXT;
	lexer.char_info['>'] = CHAR_INFO_NOT_TEXT;
	lexer.char_info['&'] = CHAR_INFO_NOT_TEXT;
	lexer.char_info['\''] = CHAR_INFO_NOT_TEXT;
	lexer.char_info['"'] = CHAR_INFO_NOT_TEXT;

	lexer.char_info['{'] = CHAR_INFO_NOT_TEXT;
	lexer.char_info['}'] = CHAR_INFO_NOT_TEXT;
	lexer.char_info['_'] = CHAR_INFO_IDENTIFIER;

	for (i='A'; i<='Z'; ++i) {
		lexer.char_info[i] = CHAR_INFO_IDENTIFIER;
	}

	for (i='a'; i<='z'; ++i) {
		lexer.char_info[i] = CHAR_INFO_IDENTIFIER;
	}

	for (i='0'; i<='9'; ++i) {
		lexer.char_info[i] = CHAR_INFO_NUMBER;
	}


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

	lexer.current = in_data;
	lexer.end = in_data + in_size;

	/* process all tokens */
	while (lexer.current < lexer.end) {
		processed = read_token(&lexer);

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

	if (__builtin_expect(lexer.exception_pending, 0)) {
		const utf32_t *p;
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
				lexer.exception_msg, line_num+1, column_num+1);
		rc = -1;
	}

	unicode_utf32_string_free(lexer.keyword_data, KEYWORD_END);
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
		|| read_token_char(lexer, '/',  HTML_TOKEN_SLASH)
		|| read_token_keyword(lexer, KEYWORD_HTML, HTML_TOKEN_HTML)
		|| read_token_keyword(lexer, KEYWORD_DATA, HTML_TOKEN_DATA)
		|| read_token_keyword(lexer, KEYWORD_INCLUDE, HTML_TOKEN_INCLUDE)
		|| read_token_identifier(lexer)
		|| read_token_whitespace(lexer)
		|| read_token_text(lexer);
}

static int read_token_identifier(struct html_lexer_t *restrict lexer)
{
	const utf32_t *restrict p = lexer->current;

	if (__builtin_expect(char_type_check(lexer, *p, CHAR_INFO_IDENTIFIER) == 0, 0))
		return 0;

	++p;
	while (char_type_check(lexer, *p, CHAR_INFO_IDENTIFIER | CHAR_INFO_NUMBER))
		++p;

	add_token(lexer, HTML_TOKEN_IDENTIFIER, lexer->current, p);
	lexer->current = p;
	return 1;
}

static int read_token_text(struct html_lexer_t *restrict lexer)
{
	const utf32_t *restrict p = lexer->current;

	/* According to https://html.spec.whatwg.org/#writing-xhtml-documents, the
	 * five special characters in HTML are: '<', '>', '&', '\'', '"'.
	 *
	 * Since we expect variable references to appear in the text, we will need
	 * to consider this regex as well: "\s*[{]\s*[A-Za-z_][A-Za-z0-9_]*\s*[}]\s".
	 * This regex contains the following tokens:
	 *  - whitespace
	 *  - open-brace
	 *  - close-brace
	 *  - identifier
	 *
	 * Since we only care about the start of the next token, we will only consider
	 * [A-Za-z_] pattern of the identifier token.
	 */

	const int flag = CHAR_INFO_NOT_TEXT | CHAR_INFO_WHITESPACE | CHAR_INFO_IDENTIFIER;

	while (!char_type_check(lexer, *p, flag))
		++p;

	if (p - lexer->current) {
		add_token(lexer, HTML_TOKEN_TEXT, lexer->current, p);
		lexer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_whitespace(struct html_lexer_t *restrict lexer)
{
	const utf32_t *restrict p = lexer->current;

	while (char_type_check(lexer, *p, CHAR_INFO_WHITESPACE))
		++p;

	if (p - lexer->current) {
		add_token(lexer, HTML_TOKEN_WHITESPACE, lexer->current, p);
		lexer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_char(struct html_lexer_t *restrict lexer, char ch, html_token_id_t token_id)
{
	const utf32_t *restrict p = lexer->current;

	if (*lexer->current == ch) {
		++p;
		add_token(lexer, token_id, lexer->current, p);
		lexer->current = p;
		return 1;
	}

	return 0;
}

static int read_token_keyword(
		struct html_lexer_t *restrict lexer, keyword_id_t keyword_id, html_token_id_t token_id)
{
	size_t size = lexer->keyword_size[keyword_id];
	const utf32_t *restrict p = lexer->current;

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
	const utf32_t *restrict p = lexer->current;
	utf32_t quote = *p;

	if (__builtin_expect(quote != '"' && quote != '\'', 0))
		return 0;

	++p;
	while (p < lexer->end) {
		if (*p == '\\') {
			p += 2;
		}
		else if (*p == quote) {
			add_token(lexer, HTML_TOKEN_STRING, lexer->current+1, p);
			lexer->current = p+1;
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
		html_token_id_t token_id)
{
	const utf32_t *restrict p = lexer->current;
	const utf32_t *keyword_data = lexer->keyword_data[keyword_begin];
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

static void add_token(
		struct html_lexer_t *restrict lexer, html_token_id_t id,
		const utf32_t *begin, const utf32_t *end)
{
	struct html_tokens_t *restrict tokens = lexer->tokens;
	size_t i = tokens->count;

	if (i < HTML_PARSER_MAX_TOKENS) {
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


