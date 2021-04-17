// SPDX-License-Identifier: GPL-2.0-only
/*
 * html_parser.c
 *
 * Copyright (C) 2021  Imran Haider
 */

#include <html_parser.h>
#include <html_lexer.h>
#include <stdio.h>
#include <assert.h>

#include <string.h>
#include <stdlib.h>

//#define TRACE_TOKENS


#define EXPECT_TOKEN_1_1(tree,p,token) \
	do {\
		if (__builtin_expect(tree->tokens.id[p] != token, 0))\
			return 0;\
		++p;\
	} while(0)

#define EXPECT_TOKEN_2_1(tree,p,token1,token2) \
	do {\
		if (__builtin_expect(tree->tokens.id[p] != token1 && \
					         tree->tokens.id[p] != token2, 0))\
			return 0;\
		++p;\
	} while(0)

#define EXPECT_TOKEN_1_0N(tree,p,token) \
	do {\
		while (tree->tokens.id[p] == token) {\
			++p;\
		}\
	} while(0)

#define EXPECT_TOKEN_2_0N(tree,p,token1,token2) \
	do {\
		while (tree->tokens.id[p] == token1 || tree->tokens.id[p] == token2) {\
			++p;\
		}\
	} while(0)

#define EXPECT_TOKEN_3_0N(tree,p,token1,token2,token3) \
	do {\
		while (tree->tokens.id[p] == token1 || tree->tokens.id[p] == token2 || tree->tokens.id[p] == token3) {\
			++p;\
		}\
	} while(0)

struct html_parser_t {
	html_token_idx_t current;

	/* parse tree */
	struct html_tree_t *restrict tree;

	/* node stack */
	html_token_idx_t node_stack[HTML_PARSER_MAX_STACK_SIZE];
	size_t node_stack_size;

	/* parsing error handling */
	const char *restrict exception_msg;
	html_token_idx_t exception_location;

	/* flags */
	unsigned int exception_pending :1;
};

struct html_builder_t {
	const struct html_tokens_t *restrict tokens;
	utf32_t *restrict output;
	size_t current;
	size_t max_size;
};

/* Lexical token analyzers */
static int read_node(struct html_parser_t *restrict parser);
static int read_node_open_tag_attributes(struct html_parser_t *restrict parser, html_token_idx_t node, html_token_idx_t p);
static int read_node_open_tag(struct html_parser_t *restrict parser);
static int read_node_close_tag(struct html_parser_t *restrict parser);
static int read_node_variable(struct html_parser_t *restrict parser);
static int read_node_text(struct html_parser_t *restrict parser);
static int read_node_whitespace(struct html_parser_t *restrict parser);

/* Parse tree operations */
static void pop_node(struct html_parser_t *restrict parser, html_token_idx_t tag_name);
static void push_node(struct html_parser_t *restrict parser, html_token_idx_t tag_name);

/* Debugging */
static void dump_parse_table(struct html_parser_t *restrict parser);

#ifdef TRACE_TOKENS
static void trace_token(html_token_id_t token_id, const utf32_t *restrict in_data, size_t in_size);
#endif


int html_parse(const utf32_t *restrict in_data, size_t in_size, struct html_tree_t *restrict tree)
{
	struct html_parser_t parser = { 0 };
	int processed;

	int rc = html_lex(in_data, in_size, &tree->tokens);
	if (__builtin_expect(rc != 0, 0)) {
		return rc;
	}

	parser.tree = tree;

	/* setting the initial stack size to 1. index 0 is reserved so that we needn't check if
	 * node_stack_size-1 is negative in push_node()
	 */
	parser.node_stack_size = 1;

	/* process all tokens */
	while (parser.current < tree->tokens.count) {
		processed = read_node(&parser);

		if (processed) {
			if (__builtin_expect(parser.exception_pending, 0))
				break;
		}
		else {
			parser.exception_pending = 1;
			parser.exception_msg = "Invalid syntax";
			parser.exception_location = parser.current;
			break;
		}
	}

	if (__builtin_expect(parser.exception_pending, 0)) {
		const utf32_t *p;
		const utf32_t *begin = tree->tokens.begin[parser.exception_location];
		int column_num = 0;
		int line_num = 0;

		for (p = in_data; p < begin; ++p) {
			if (*p != '\n') {
				++column_num;
			}
			else {
				++line_num;
				column_num = 0;
			}
		}

		fprintf(stderr, "html_parse: %s on line %d, column %d\n",
				parser.exception_msg, line_num+1, column_num+1);
		rc = -1;
	}

	dump_parse_table(&parser);

	return rc;
}

static int read_node(struct html_parser_t *restrict parser)
{
#ifdef TRACE_TOKENS
	struct html_tokens_t *restrict tokens = &parser->tree->tokens;
	trace_token(
			tokens->id[parser->current], tokens->begin[parser->current],
			tokens->end[parser->current] - tokens->begin[parser->current]);
	++parser->current;
	return 1;
#endif

	return
		   read_node_open_tag(parser)
		|| read_node_close_tag(parser)
		|| read_node_variable(parser)
		|| read_node_text(parser)
		|| read_node_whitespace(parser);
}


static int read_node_open_tag_attributes(struct html_parser_t *restrict parser, html_token_idx_t node, html_token_idx_t p)
{
	struct html_tree_t *restrict tree = parser->tree;
	html_token_idx_t attrib_name = p;
	html_token_idx_t attrib_value = 0;
	size_t attrib_idx = tree->attrib_count;

	while (tree->tokens.id[p] == HTML_TOKEN_IDENTIFIER) {
		++p;
		EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);

		if (tree->tokens.id[p] == HTML_TOKEN_EQUAL) {
			++p;

			EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);
			attrib_value = p;
			EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_STRING);
			EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);
		}
		else {
			attrib_value = 0;
		}

		tree->attrib_parent[attrib_idx] = node;
		tree->attrib_name[attrib_idx] = attrib_name;
		tree->attrib_value[attrib_idx] = attrib_value;
		++attrib_idx;

		attrib_name = p;
	}

	EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_GREATERTHAN);

	tree->attrib_count = attrib_idx;
	parser->current = p;

	return 1;
}

static int read_node_open_tag(struct html_parser_t *restrict parser)
{
	struct html_tree_t *restrict tree = parser->tree;

	html_token_idx_t p = parser->current;
	html_token_idx_t tag_name = 0;

	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_LESSTHAN);
	tag_name = p;
	EXPECT_TOKEN_2_1(tree, p, HTML_TOKEN_IDENTIFIER, HTML_TOKEN_HTML);
	EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);

	/* By this time, we are pretty certain that we are parsing a tag. If we are wrong, we'll
	 * reset the state to undo the push_node operation
	 */
	push_node(parser, tag_name);

	if (read_node_open_tag_attributes(parser, tag_name, p)) {
		return 1;
	}

	--parser->tree->node_count;
	--parser->node_stack_size;

	return 0;
}

static int read_node_close_tag(struct html_parser_t *restrict parser)
{
	struct html_tree_t *restrict tree = parser->tree;

	html_token_idx_t p = parser->current;
	html_token_idx_t tag_name = 0;

	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_LESSTHAN);
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_SLASH);
	tag_name = p;
	EXPECT_TOKEN_2_1(tree, p, HTML_TOKEN_IDENTIFIER, HTML_TOKEN_HTML);
	EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_GREATERTHAN);

	pop_node(parser, tag_name);

	parser->current = p;
	return 1;
}

static int read_node_variable(struct html_parser_t *restrict parser)
{
	struct html_tree_t *restrict tree = parser->tree;

	html_token_idx_t p = parser->current;
	html_token_idx_t var_name = 0;

	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_OPENBRACE);
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_OPENBRACE);
	EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);
	var_name = p;
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_IDENTIFIER);
	EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_CLOSEBRACE);
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_CLOSEBRACE);

	push_node(parser, var_name);
	pop_node(parser, var_name);

	parser->current = p;
	return 1;
}

static int read_node_text(struct html_parser_t *restrict parser)
{
	struct html_tree_t *restrict tree = parser->tree;

	html_token_idx_t p = parser->current;
	EXPECT_TOKEN_3_0N(tree, p, HTML_TOKEN_TEXT, HTML_TOKEN_WHITESPACE, HTML_TOKEN_IDENTIFIER);

	if (parser->current != p) {
		parser->current = p;
		return 1;
	}

	return 0;
}

static int read_node_whitespace(struct html_parser_t *restrict parser)
{
	struct html_tree_t *restrict tree = parser->tree;
	html_token_idx_t p = parser->current;
	EXPECT_TOKEN_1_1(tree, p, HTML_TOKEN_WHITESPACE);
	EXPECT_TOKEN_1_0N(tree, p, HTML_TOKEN_WHITESPACE);
	parser->current = p;
	return 1;
}

/* Parse tree operations */
static void pop_node(struct html_parser_t *restrict parser, html_token_idx_t tag_name)
{
	struct html_tree_t *restrict tree = parser->tree;
	html_token_id_t *restrict id = tree->tokens.id;
	int32_t i;

	for (i=parser->node_stack_size-1; i>0; --i) {
		if (id[parser->node_stack[i]] == id[tag_name]) {
			parser->node_stack_size = i;
			return;
		}
	}
}

static void push_node(struct html_parser_t *restrict parser, html_token_idx_t tag_name)
{
	struct html_tree_t *restrict tree = parser->tree;
	size_t i = tree->node_count;

	if (i < HTML_PARSER_MAX_NODES) {
		tree->node_parent[i] = parser->node_stack[parser->node_stack_size-1];
		tree->node_tag_name[i] = tag_name;
		tree->node_count = i+1;

		parser->node_stack[parser->node_stack_size] = tag_name;
		++parser->node_stack_size;
	}
	else {
		parser->exception_pending = 1;
		parser->exception_msg = "Not enough space for tree";
		parser->exception_location = tag_name;
	}
}

static char *get_token_string(const struct html_tree_t *restrict tree, html_token_idx_t id)
{
	char *str;
	size_t size = 1;
	const utf32_t *begin = tree->tokens.begin[id];
	const utf32_t *end = tree->tokens.end[id];

	unicode_write_utf8_string(begin, end-begin, &str, &size);
	str[size] = 0;

	return str;
}

static void dump_parse_table(struct html_parser_t *restrict parser)
{
	size_t i;
	struct html_tree_t *restrict tree = parser->tree;

	printf("nodes:\n");
	for (i=0; i < tree->node_count; ++i) {
		char *parent = get_token_string(tree, tree->node_parent[i]);
		char *tag_name = get_token_string(tree, tree->node_tag_name[i]);

		printf("\ttag[%s]\tparent[%s]\n", tag_name, tree->node_parent[i]? parent : "");

		unicode_utf8_string_free(&parent, 1);
		unicode_utf8_string_free(&tag_name, 1);
	}

	printf("\nattributes:\n");
	for (i=0; i < tree->attrib_count; ++i) {
		char *parent = get_token_string(tree, tree->attrib_parent[i]);
		char *name = get_token_string(tree, tree->attrib_name[i]);
		char *value = get_token_string(tree, tree->attrib_value[i]);

		printf("\tname[%s]\tvalue[%s]\tparent[%s]\n", name, tree->attrib_value[i] ? value : "true", parent);

		unicode_utf8_string_free(&parent, 1);
		unicode_utf8_string_free(&name, 1);
		unicode_utf8_string_free(&value, 1);
	}
}

#ifdef TRACE_TOKENS
static void trace_token(html_token_id_t token_id, const utf32_t *restrict in_data, size_t in_size)
{
	char *str;
	size_t size = 1;

	unicode_write_utf8_string(in_data, in_size, &str, &size);

	/* guaranteed space because the inital value of 'size' is 1 */
	str[size] = 0;

	switch (token_id) {
	case HTML_TOKEN_GREATERTHAN:
		printf("[>] '%s'\n", str);
		break;
	case HTML_TOKEN_LESSTHAN:
		printf("[<]: '%s'\n", str);
		break;
	case HTML_TOKEN_IDENTIFIER:
		printf("[identifier]: '%s'\n", str);
		break;
	case HTML_TOKEN_WHITESPACE:
		printf("[space]: '%s'\n", str);
		break;
	case HTML_TOKEN_EQUAL:
		printf("[=]: '%s'\n", str);
		break;
	case HTML_TOKEN_SINGLEQUOTE:
		printf("[']: '%s'\n", str);
		break;
	case HTML_TOKEN_DOUBLEQUOTE:
		printf("[\"]: '%s'\n", str);
		break;
	case HTML_TOKEN_AMPERSAND:
		printf("[&]: '%s'\n", str);
		break;
	case HTML_TOKEN_EXCLAMATIONMARK:
		printf("[!]: '%s'\n", str);
		break;
	case HTML_TOKEN_HYPHEN:
		printf("[-]: '%s'\n", str);
		break;
	case HTML_TOKEN_COLON:
		printf("[:]: '%s'\n", str);
		break;
	case HTML_TOKEN_OPENBRACE:
		printf("[{]: '%s'\n", str);
		break;
	case HTML_TOKEN_CLOSEBRACE:
		printf("[}] '%s'\n", str);
		break;
	case HTML_TOKEN_OPENPAREN:
		printf("[(]: '%s'\n", str);
		break;
	case HTML_TOKEN_CLOSEPAREN:
		printf("[)]: '%s'\n", str);
		break;
	case HTML_TOKEN_SEMICOLON:
		printf("[;]: '%s'\n", str);
		break;
	case HTML_TOKEN_ASTERISK:
		printf("[*]: '%s'\n", str);
		break;
	case HTML_TOKEN_HASH:
		printf("[#]: '%s'\n", str);
		break;
	case HTML_TOKEN_COMMA:
		printf("[,]: '%s'\n", str);
		break;
	case HTML_TOKEN_SLASH:
		printf("[/]: '%s'\n", str);
		break;
	case HTML_TOKEN_HTML:
		printf("[html]: '%s'\n", str);
		break;
	case HTML_TOKEN_DATA:
		printf("[data]: '%s'\n", str);
		break;
	case HTML_TOKEN_SCRIPT:
		printf("[script]: '%s'\n", str);
		break;
	case HTML_TOKEN_STRING:
		printf("[string]: '%s'\n", str);
		break;
	case HTML_TOKEN_TEXT:
		printf("[text]: '%s'\n", str);
		break;
	case HTML_TOKEN_COMMENT:
		printf("[comment]: '%s'\n", str);
		break;
	case HTML_TOKEN_STYLE:
		printf("[style]: '%s'\n", str);
		break;
	}

	unicode_utf8_string_free(&str, 1);
}
#endif

static void append_token(struct html_builder_t *builder, html_token_idx_t token_idx)
{
	const utf32_t *begin = builder->tokens->begin[token_idx];
	size_t size = builder->tokens->end[token_idx] - begin;
	memcpy(builder->output + builder->current, begin, size * sizeof(utf32_t));
	builder->current += size;
	builder->max_size -= size;
}

void html_build(
		utf32_t *restrict *restrict out_data, size_t *restrict out_size,
		const struct html_tree_t *restrict tree)
{
	/* create a lookup table to locate an instance of a token given its id */
	html_token_idx_t token_idx[HTML_TOKEN_END];
	const struct html_tokens_t *restrict tokens = &tree->tokens;
	html_token_idx_t idx;

	for (idx=0; idx < tokens->count; ++idx) {
		html_token_id_t id = tokens->id[idx];
		token_idx[id] = idx;
	}

	/* node stack */
	html_token_idx_t node_stack[HTML_PARSER_MAX_STACK_SIZE];
	size_t node_stack_size = 0;

	/* allocate memory for output data */
	*out_size = HTML_PARSER_MAX_SIZE;
	*out_data = malloc(HTML_PARSER_MAX_SIZE * sizeof(utf32_t));

	size_t i;
	struct html_builder_t builder = {0};

	builder.tokens = tokens;
	builder.output = *out_data;
	builder.max_size = HTML_PARSER_MAX_SIZE;

	for (i=0; i < tree->node_count; ++i) {
		html_token_idx_t tag_name = tree->node_tag_name[i];
		html_token_idx_t parent = tree->node_parent[i];

		/* write closing tags if we moved to a sibling node or a parent node */
		while (node_stack_size > 0 && parent != node_stack[node_stack_size-1]) {
			append_token(&builder, token_idx[HTML_TOKEN_LESSTHAN]);
			append_token(&builder, token_idx[HTML_TOKEN_SLASH]);
			append_token(&builder, node_stack[node_stack_size-1]);
			append_token(&builder, token_idx[HTML_TOKEN_GREATERTHAN]);

			--node_stack_size;
		}

		append_token(&builder, token_idx[HTML_TOKEN_LESSTHAN]);
		append_token(&builder, tag_name);
		append_token(&builder, token_idx[HTML_TOKEN_GREATERTHAN]);

		node_stack[node_stack_size] = tag_name;
		++node_stack_size;
	}

	for (; node_stack_size > 0; --node_stack_size) {
		append_token(&builder, token_idx[HTML_TOKEN_LESSTHAN]);
		append_token(&builder, token_idx[HTML_TOKEN_SLASH]);
		append_token(&builder, node_stack[node_stack_size-1]);
		append_token(&builder, token_idx[HTML_TOKEN_GREATERTHAN]);
	}


	*out_size = builder.current;
}



