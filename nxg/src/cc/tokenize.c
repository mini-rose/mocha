#include <ctype.h>
#include <memory.h>
#include <nxg/cc/alloc.h>
#include <nxg/cc/keyword.h>
#include <nxg/cc/tokenize.h>
#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <nxg/utils/utils.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void token_list_append(token_list *list, token *tok)
{
	list->tokens = realloc_ptr_array(list->tokens, ++list->length);
	list->tokens[list->length - 1] = tok;
}

token_list *token_list_new()
{
	token_list *list = (token_list *) slab_alloc(sizeof(token_list));
	list->tokens = NULL;
	list->length = 0;
	list->iter = 0;
	return list;
}

token *token_new(token_t type, const char *value, int len)
{
	token *tok = slab_alloc(sizeof(token));
	tok->type = type;
	tok->value = value;
	tok->len = len;
	return tok;
}

void token_print(token *tok)
{
	static const char *tok_str[] = {[T_DATATYPE] = "DATATYPE",
					[T_NEWLINE] = "NEWLINE",
					[T_KEYWORD] = "KEYWORD",
					[T_NUMBER] = "NUMBER",
					[T_STRING] = "STRING",
					[T_IDENT] = "IDENT",
					[T_PUNCT] = "PUNCT",
					[T_END] = "END",
					[T_ASS] = "ASS",
					[T_EQ] = "EQ",
					[T_NEQ] = "NEQ",
					[T_ADDA] = "ADDA",
					[T_ARROW] = "ARROW",
					[T_DEC] = "DEC",
					[T_TRUE] = "TRUE",
					[T_FALSE] = "FALSE",
					[T_INC] = "INC",
					[T_SUBA] = "SUBA",
					[T_ADD] = "ADD",
					[T_DIV] = "DIV",
					[T_MOD] = "MOD",
					[T_DIVA] = "DIVA",
					[T_MODA] = "MODA",
					[T_LPAREN] = "LPAREN",
					[T_RPAREN] = "RPAREN",
					[T_COMMA] = "COMMA",
					[T_LBRACE] = "LBRACE",
					[T_RBRACE] = "RBRACE",
					[T_DOT] = "DOT",
					[T_MUL] = "MUL",
					[T_SUB] = "SUB"};

	if (tok->type == T_NEWLINE)
		printf("\033[37m%s\033[0m", tok_str[tok->type]);
	else if (tok->type == T_END)
		printf("%s ", tok_str[tok->type]);
	else if (tok->type == T_TRUE)
		printf("%s", tok_str[tok->type]);
	else if (tok->type == T_FALSE)
		printf("%s", tok_str[tok->type]);
	else if (tok->type == T_KEYWORD)
		printf("\033[31m%s\033[0m: \033[32m'%.*s'\033[0m",
		       tok_str[tok->type], tok->len, tok->value);
	else if (tok->type == T_DATATYPE)
		printf("\033[34m%s\033[0m: \033[32m'%.*s'\033[0m",
		       tok_str[tok->type], tok->len, tok->value);
	else if (tok->type == T_NUMBER)
		printf("\033[33m%s\033[0m: \033[32m'%.*s'\033[0m",
		       tok_str[tok->type], tok->len, tok->value);
	else
		printf("%s: \033[32m'%.*s'\033[0m", tok_str[tok->type],
		       tok->len, tok->value);
}

void token_list_print(token_list *list)
{
	puts("TOKENS: [");
	for (int i = 0; i < list->length; i++) {
		fputs("\t", stdout);
		token_print(list->tokens[i]);

		if (i + 1 != list->length)
			fputs(",\n", stdout);
	}
	puts("\n]");
}

static char *push_str(const char *start, const char *end)
{
	size_t size = end - start + 1;
	char *buf = slab_alloc(size);
	snprintf(buf, size, "%.*s", (int) (end - start), start);
	return buf;
}

static char *strend(char *p)
{
	char *q = p++;

	for (; *p != *q; p++) {
		if (*p == '\n' || *p == '\0')
			return NULL;

		if (*p == '\\')
			p++;
	}

	return p;
}

token_list *tokens(file_t *source)
{
	token_t last = T_NEWLINE;
	token_list *list = token_list_new();
	file_t *f = source;
	list->source = source;
	char *p = f->content;

	while (*p) {
		/* Skip long comment */
		if (!strncmp(p, "/*", 2)) {
			char *q = strstr(p + 2, "*/");

			if (!q)
				error_at(f, p, 1, "Unterminated comment.");

			p += q - p + 2;
			continue;
		}

		/* Skip oneline comment */
		if (!strncmp(p, "//", 2)) {
			char *q = strstr(p + 2, "\n");
			p += q - p;
		}

		/* Parse newline token,
		   but skip when previous token is newline */
		if (*p == '\n') {
			if (last == T_NEWLINE) {
				p++;
				continue;
			} else {
				token *tok = token_new(last = T_NEWLINE, p, 0);
				token_list_append(list, tok);
				p++;
				continue;
			}
		}

		/* Skip space */
		if (isspace(*p)) {
			p++;
			continue;
		}

		/* Tokenize quoted string */
		if (*p == '\'' || *p == '"') {
			token *tok;
			char *q = strend(p++);

			if (!q)
				error_at(f, p - 1, 1,
					 "Unterminated quoted string.");

			tok = token_new(last = T_STRING, p, q - p);
			token_list_append(list, tok);

			p += q - p + 1;
			continue;
		}

		/* Tokenize string and quess it type */
		if (isalpha(*p) || *p == '_') {
			token *tok = NULL;
			char *str;
			int strl;
			char *q = p;

			while (isalnum(*q) || *q == '_')
				q++;

			str = push_str(p, q);
			strl = strlen(str);

			if (!strncmp("true", str, strl)) {
				tok = token_new(last = T_TRUE, p, p - q);
			} else if (!strncmp("false", str, strl)) {
				tok = token_new(last = T_FALSE, p, p - q);
			} else if (is_keyword(str)) {
				tok = token_new(last = T_KEYWORD, p, q - p);
			} else if (!strncmp("__LINE__", str, strl)) {
				int line = 1;
				static char buf[12];

				for (char *c = f->content; c < p; c++)
					if (*c == '\n')
						line++;

				snprintf(buf, sizeof(buf), "%i", line);

				tok = token_new(last = T_NUMBER, buf,
						strlen(buf));

				token_list_append(list, tok);

				p += 8;
				continue;
			} else if (!strncmp("stdin", str, strl)) {
				char *n = "0";
				tok = token_new(last = T_NUMBER, n, 1);
				token_list_append(list, tok);
				p += 5;
				continue;
			} else if (!strncmp("stdout", str, strl)) {
				char *n = "1";
				tok = token_new(last = T_NUMBER, n, 1);
				token_list_append(list, tok);
				p += 6;
				continue;
			} else if (!strncmp("stderr", str, strl)) {
				char *n = "2";
				tok = token_new(last = T_NUMBER, n, 1);
				token_list_append(list, tok);
				p += 6;
				continue;
			} else if (is_plain_type(str)) {
				tok = token_new(last = T_DATATYPE, p, q - p);
			} else
				tok = token_new(last = T_IDENT, p, q - p);

			token_list_append(list, tok);
			p += q - p;
			continue;
		}

		/* Tokenize number */
		if (isdigit(*p) || (*p == '.' && isdigit(*(p + 1)))) {
			token *tok;
			char *q = p;

			if (*q == '.')
				q++;

			while (isdigit(*q) || *q == '.')
				q++;

			tok = token_new(last = T_NUMBER, p, q - p);
			token_list_append(list, tok);

			p += q - p;
			continue;
		}

		/* Tokenize punct and quess it type */
		if (ispunct(*p)) {
			token *tok = NULL;

			static char *operators[] = {
			    [T_ASS] = "=",   [T_EQ] = "==",    [T_NEQ] = "!=",
			    [T_ADDA] = "+=", [T_ARROW] = "->", [T_DEC] = "--",
			    [T_INC] = "++",  [T_SUBA] = "-=",  [T_ADD] = "+",
			    [T_DIV] = "/",   [T_MOD] = "%",    [T_DIVA] = "/=",
			    [T_MODA] = "%=", [T_MUL] = "*",    [T_SUB] = "-"};

			switch (*p) {
			case '(':
				tok = token_new(last = T_LPAREN, p, 1);
				token_list_append(list, tok);
				p++;
				break;
			case ')':
				tok = token_new(last = T_RPAREN, p, 1);
				token_list_append(list, tok);
				p++;
				break;
			case '{':
				tok = token_new(last = T_LBRACE, p, 1);
				token_list_append(list, tok);
				p++;
				break;
			case '}':
				tok = token_new(last = T_RBRACE, p, 1);
				token_list_append(list, tok);
				p++;
				break;
			case ',':
				tok = token_new(last = T_COMMA, p, 1);
				token_list_append(list, tok);
				p++;
				break;
			case '.':
				tok = token_new(last = T_DOT, p, 1);
				token_list_append(list, tok);
				p++;
				break;
			case ';':
				p++;
				break;
			default:
				break;
			}

			if (!ispunct(*p))
				continue;

			for (int i = T_EQ; i < LEN(operators); i++) {
				if (!strncmp(p, operators[i],
					     strlen(operators[i]))) {

					tok = token_new(last = (token_t) i, p,
							strlen(operators[i]));

					token_list_append(list, tok);
					p += strlen(operators[i]);
					break;
				}
			}

			if (tok)
				continue;

			while (isspace(*p))
				p++;

			if (!ispunct(*p) || *p == '"')
				continue;

			tok = token_new(last = T_PUNCT, p, 1);
			token_list_append(list, tok);

			p++;
			continue;
		}


		p++;
	}

	/* Add end token */
	token *tok = token_new(T_END, p, 0);
	token_list_append(list, tok);

	return list;
}

const char *tokname(token_t toktype)
{
	static const char *toknames[] = {"operator",   "datatype",    "newline",
					 "keyword",    "number",      "string",
					 "identifier", "punctuation", "end"};
	if (toktype < 0 || toktype >= LEN(toknames))
		return "<tokname() failed>";
	return toknames[toktype];
}
