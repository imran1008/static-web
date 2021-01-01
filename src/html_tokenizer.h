/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * html_tokenizer.h
 *
 * Copyright (C) 2021  Imran Haider
 */

#ifndef HTML_TOKENIZER_H
#define HTML_TOKENIZER_H

#include <html_parser.h>
#include <stddef.h>
#include <stdint.h>

int html_tokenize(
		const int32_t *restrict in_data, size_t in_size, struct html_tokens_t *restrict tokens);


#endif
