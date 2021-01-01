// SPDX-License-Identifier: GPL-2.0-only
/*
 * html_parser.c
 *
 * Copyright (C) 2021  Imran Haider
 */

#include <html_parser.h>
#include <html_tokenizer.h>

int html_parse(const int32_t *restrict in_data, size_t in_size)
{
	struct html_tokens_t tokens = { 0 };
	tokens.capacity = HTML_PARSER_MAX_TOKENS;

	int rc = html_tokenize(in_data, in_size, &tokens);
	return rc;
}


