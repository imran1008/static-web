/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * html_lexer.h
 *
 * Copyright (C) 2021  Imran Haider
 */

#ifndef HTML_LEXER_H
#define HTML_LEXER_H

#include <html_parser.h>
#include <stddef.h>
#include <stdint.h>

int html_lex(
		const int32_t *restrict in_data, size_t in_size, struct html_tokens_t *restrict tokens);


#endif
