/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * html_parser.h
 *
 * Copyright (C) 2021  Imran Haider
 */

#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include <unicode.h>
#include <stddef.h>
#include <stdint.h>

#define HTML_PARSER_MAX_TOKENS      2048
#define HTML_PARSER_MAX_NODES       1024
#define HTML_PARSER_MAX_ATTRIBUTES  2048
#define HTML_PARSER_MAX_SIZE        65536    /* in characters */
#define HTML_PARSER_MAX_STACK_SIZE  1000

enum {
	HTML_TOKEN_GREATERTHAN,
	HTML_TOKEN_LESSTHAN,
	HTML_TOKEN_IDENTIFIER,
	HTML_TOKEN_WHITESPACE,
	HTML_TOKEN_EQUAL,
	HTML_TOKEN_SINGLEQUOTE,
	HTML_TOKEN_DOUBLEQUOTE,
	HTML_TOKEN_AMPERSAND,
	HTML_TOKEN_EXCLAMATIONMARK,
	HTML_TOKEN_HYPHEN,
	HTML_TOKEN_COLON,
	HTML_TOKEN_OPENBRACE,
	HTML_TOKEN_CLOSEBRACE,
	HTML_TOKEN_OPENPAREN,
	HTML_TOKEN_CLOSEPAREN,
	HTML_TOKEN_SEMICOLON,
	HTML_TOKEN_ASTERISK,
	HTML_TOKEN_HASH,
	HTML_TOKEN_COMMA,
	HTML_TOKEN_SLASH,
	HTML_TOKEN_HTML,
	HTML_TOKEN_DATA,
	HTML_TOKEN_SCRIPT,
	HTML_TOKEN_STRING,
	HTML_TOKEN_TEXT,
	HTML_TOKEN_COMMENT,
	HTML_TOKEN_STYLE,
	HTML_TOKEN_INCLUDE,
	HTML_TOKEN_END
};

/* Must be less than HTML_TOKEN_END */
typedef uint8_t html_token_id_t;

/* Must be less than HTML_PARSER_MAX_TOKENS */
typedef uint16_t html_token_idx_t;

struct html_tokens_t {
	const utf32_t *begin[HTML_PARSER_MAX_TOKENS];
	const utf32_t *end[HTML_PARSER_MAX_TOKENS];
	html_token_id_t id[HTML_PARSER_MAX_TOKENS];
	size_t count;
};

struct html_tree_t {
	struct html_tokens_t tokens;

	html_token_idx_t node_parent[HTML_PARSER_MAX_NODES];
	html_token_idx_t node_tag_name[HTML_PARSER_MAX_NODES];

	html_token_idx_t attrib_parent[HTML_PARSER_MAX_ATTRIBUTES];
	html_token_idx_t attrib_name[HTML_PARSER_MAX_ATTRIBUTES];
	html_token_idx_t attrib_value[HTML_PARSER_MAX_ATTRIBUTES];

	size_t attrib_count;
	size_t node_count;
};

int html_parse(const utf32_t *restrict in_data, size_t in_size, struct html_tree_t *restrict tree);
void html_build(
		utf32_t *restrict *restrict out_data, size_t *restrict out_size,
		const struct html_tree_t *restrict tree);

#endif

