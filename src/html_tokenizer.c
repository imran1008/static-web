#include <html_tokenizer.h>
#include <unicode.h>
#include <string.h>

enum {
	TOKEN_GREATERTHAN,
	TOKEN_LESSTHAN,
	TOKEN_WORD,
	TOKEN_WHITESPACE,
	TOKEN_EQUAL,
	TOKEN_SINGLEQUOTE,
	TOKEN_DOUBLEQUOTE,
	TOKEN_AMPERSAND,
	TOKEN_EXCLAMATIONMARK,
	TOKEN_HYPHEN,
	TOKEN_COLON,
	TOKEN_OPENBRACE,
	TOKEN_CLOSEBRACE,
	TOKEN_OPENPAREN,
	TOKEN_CLOSEPAREN,
	TOKEN_SEMICOLON,
	TOKEN_ASTERISK,
	TOKEN_HASH,
	TOKEN_COMMA,
	TOKEN_HTML,
	TOKEN_DATA,
	TOKEN_SCRIPT,
	TOKEN_STRING,
	TOKEN_END
};

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

struct tokens_t {
	/* TODO */
};

struct keywords_t {
	int   *data[KEYWORD_END];
	size_t size[KEYWORD_END];
};

static int read_token_word(struct tokens_t *tokens, const int **s);
static int read_token_whitespace(struct tokens_t *tokens, const int **s);
static int read_token_char(struct tokens_t *tokens, const int **s, char ch, int id);
static int read_token_keyword(
		struct tokens_t *tokens, const int **s, const struct keywords_t *keywords, int keyword_id, int token_id);
static int read_token_string(struct tokens_t *tokens, const int **s);
static int read_token_cdata(
		struct tokens_t *tokens, const int **s, const struct keywords_t *keywords, size_t input_size,
		size_t keyword_start, size_t keyword_end);
static void add_token(struct tokens_t *tokens, int id, const int *begin, const int *end);


void html_tokenizer_process(const int *restrict in_data, size_t in_size)
{
	struct tokens_t tokens;
	struct keywords_t keywords = {0};

	const int *p = in_data;
	const int *q = in_data + in_size;

#define REGISTER_KEYWORD(id,str) unicode_read_ascii_string(str, sizeof(str)-1, keywords.data+id, keywords.size+id)
	REGISTER_KEYWORD(KEYWORD_HTML,          "html");
	REGISTER_KEYWORD(KEYWORD_DATA,          "data");
	REGISTER_KEYWORD(KEYWORD_INCLUDE,       "include");
	REGISTER_KEYWORD(KEYWORD_SCRIPT_START,  "<script");
	REGISTER_KEYWORD(KEYWORD_SCRIPT_END,    "</script>");
	REGISTER_KEYWORD(KEYWORD_STYLE_START,   "<style");
	REGISTER_KEYWORD(KEYWORD_STYLE_END,     "</style>");
	REGISTER_KEYWORD(KEYWORD_COMMENT_START, "<!--");
	REGISTER_KEYWORD(KEYWORD_COMMENT_END,   "-->");


	while (p < q) {
		if (
			read_token_cdata(&tokens, &p, &keywords, q-p, KEYWORD_COMMENT_START, KEYWORD_COMMENT_END)  ||
			read_token_string(&tokens, &p) ||
			read_token_cdata(&tokens, &p, &keywords, q-p, KEYWORD_SCRIPT_START, KEYWORD_SCRIPT_END)  ||
			read_token_cdata(&tokens, &p, &keywords, q-p, KEYWORD_STYLE_START, KEYWORD_STYLE_END)  ||

			read_token_char(&tokens, &p, '>',  TOKEN_GREATERTHAN)     ||
			read_token_char(&tokens, &p, '<',  TOKEN_LESSTHAN)        ||
			read_token_char(&tokens, &p, '\'', TOKEN_SINGLEQUOTE)     ||
			read_token_char(&tokens, &p, '"',  TOKEN_DOUBLEQUOTE)     ||
			read_token_char(&tokens, &p, '&',  TOKEN_AMPERSAND)       ||
			read_token_char(&tokens, &p, '!',  TOKEN_EXCLAMATIONMARK) ||
			read_token_char(&tokens, &p, '=',  TOKEN_EQUAL)           ||
			read_token_char(&tokens, &p, '-',  TOKEN_HYPHEN)          ||
			read_token_char(&tokens, &p, ':',  TOKEN_COLON)           ||
			read_token_char(&tokens, &p, '{',  TOKEN_OPENBRACE)       ||
			read_token_char(&tokens, &p, '}',  TOKEN_CLOSEBRACE)      ||
			read_token_char(&tokens, &p, '(',  TOKEN_OPENPAREN)       ||
			read_token_char(&tokens, &p, ')',  TOKEN_CLOSEPAREN)      ||
			read_token_char(&tokens, &p, ';',  TOKEN_SEMICOLON)       ||
			read_token_char(&tokens, &p, '*',  TOKEN_ASTERISK)        ||
			read_token_char(&tokens, &p, '#',  TOKEN_HASH)            ||
			read_token_char(&tokens, &p, ',',  TOKEN_COMMA)           ||

			read_token_keyword(&tokens, &p, &keywords, 0, TOKEN_HTML) ||
			read_token_keyword(&tokens, &p, &keywords, 1, TOKEN_DATA) ||

			read_token_word(&tokens, &p)        ||
			read_token_whitespace(&tokens, &p)) {
			continue;
		}
	}

	unicode_string_free(keywords.data, sizeof(keywords.data) / sizeof(keywords.data[0]));
}

static int read_token_word(struct tokens_t *tokens, const int **s)
{
	const int *p = *s;

	if (__builtin_expect((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && *p != '_', 0))
		return 0;

	++p;
	while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')
		++p;

	add_token(tokens, TOKEN_WORD, *s, p);
	*s = p;

	return 1;
}

static int read_token_whitespace(struct tokens_t *tokens, const int **s)
{
	const int *p = *s;

	while (*p == ' ' || *p == '\n' || *p == '\t')
		++p;

	if (p - *s) {
		add_token(tokens, TOKEN_WHITESPACE, *s, p);
		*s = p;
		return 1;
	}

	return 0;
}

static int read_token_char(struct tokens_t *tokens, const int **s, char ch, int id)
{
	if (**s == ch) {
		add_token(tokens, id, *s, *s+1);
		++*s;
		return 1;
	}

	return 0;
}

static int read_token_keyword(
		struct tokens_t *tokens, const int **s, const struct keywords_t *keywords, int keyword_id, int token_id)
{
	if (unicode_compare_likely_equal(*s, keywords->data[keyword_id], keywords->size[keyword_id]) == 0) {
		add_token(tokens, token_id, *s, *s+1);
		++*s;
		return 1;
	}

	return 0;
}

static int read_token_string(struct tokens_t *tokens, const int **s)
{
	const int *p = *s;

	if (*p == '"') {
		while (*(++p) != '"') {
			if (*p == '\\') ++p;
		}
	}

	if (p - *s) {
		add_token(tokens, TOKEN_STRING, *s, p);
		*s = p;
		return 1;
	}

	return 0;
}

static int read_token_cdata(
		struct tokens_t *tokens, const int **s, const struct keywords_t *keywords, size_t input_size,
		size_t keyword_start, size_t keyword_end)
{

	const int *p = *s;
	if (unicode_compare_likely_equal(p, keywords->data[keyword_start], keywords->size[keyword_start]) == 0) {
		int res = unicode_find(p + keywords->size[keyword_start], keywords->data[keyword_end], input_size,
				keywords->size[keyword_end]);

		if (res > -1) {
			*s += keywords->size[keyword_start] + res;
			return 1;
		}
	}

	return 0;
}

static void add_token(struct tokens_t *tokens, int id, const int *begin, const int *end)
{
	/* TODO */
}


