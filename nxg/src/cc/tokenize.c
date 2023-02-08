#include <ctype.h>
#include <memory.h>
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

static void token_destroy(token *tok);

void token_list_append(token_list *list, token *tok)
{
	list->tokens = realloc(list->tokens, ++list->length * sizeof(token));
	list->tokens[list->length - 1] = tok;
}

token_list *token_list_new()
{
	token_list *list = (token_list *) malloc(sizeof(token_list));
	list->tokens = NULL;
	list->length = 0;
	list->iter = 0;
	return list;
}

void token_list_destroy(token_list *list)
{
	for (int i = 0; i < list->length; i++)
		token_destroy(list->tokens[i]);
	free(list->tokens);
	free(list);
}

void token_destroy(token *tok)
{
	free(tok);
}

token *token_new(token_t type, const char *value, int len)
{
	token *tok = malloc(sizeof(token));
	tok->type = type;
	tok->value = value;
	tok->len = len;
	return tok;
}

void token_print(token *tok)
{
	static const char *tok_str[] = {"OPERATOR", "DATATYPE", "NEWLINE",
					"KEYWORD",  "NUMBER",   "STRING",
					"IDENT",    "PUNCT",    "END"};

	if (tok->type == T_NEWLINE)
		printf(" \033[37m%s\033[0m ", tok_str[tok->type]);
	else if (tok->type == T_END)
		printf(" %s ", tok_str[tok->type]);
	else if (tok->type == T_KEYWORD)
		printf(" \033[31m%s\033[0m: \033[32m'%.*s'\033[0m ",
		       tok_str[tok->type], tok->len, tok->value);
	else if (tok->type == T_DATATYPE)
		printf(" \033[34m%s\033[0m: \033[32m'%.*s'\033[0m ",
		       tok_str[tok->type], tok->len, tok->value);
	else if (tok->type == T_NUMBER)
		printf(" \033[33m%s\033[0m: \033[32m'%.*s'\033[0m ",
		       tok_str[tok->type], tok->len, tok->value);
	else
		printf(" %s: \033[32m'%.*s'\033[0m ", tok_str[tok->type],
		       tok->len, tok->value);
}

void token_list_print(token_list *list)
{
	puts("TOKENS: [");
	for (int i = 0; i < list->length; i++) {
		fputs("\t{", stdout);
		token_print(list->tokens[i]);
		fputs("}", stdout);

		if (i + 1 != list->length)
			fputs(",\n", stdout);
	}
	puts("\n]");
}

static char *push_str(const char *start, const char *end)
{
	size_t size = end - start + 1;
	char *buf = calloc(1, size);
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

token_list *tokens(file *source)
{
	token_t last;
	token_list *list = token_list_new();
	file *f = source;
	list->source = source;
	char *p = f->content;

	while (*p) {
		if (!strncmp(p, "/*", 2)) {
			char *q = strstr(p + 2, "*/");

			if (!q)
				error_at(f->content, p,
					 "Unterminated comment.");

			p += q - p + 2;
			continue;
		}

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

		if (isspace(*p)) {
			p++;
			continue;
		}

		if (*p == '\'' || *p == '"') {
			token *tok;
			char *q = strend(p++);

			if (!q)
				error_at(f->content, p - 1,
					 "Unterminated quoted string.");

			tok = token_new(last = T_STRING, p, q - p);
			token_list_append(list, tok);

			p += q - p + 1;
			continue;
		}

		if (isalpha(*p) || *p == '_') {
			token *tok = NULL;
			char *str;
			char *q = p;

			while (isalnum(*q) || *q == '_')
				q++;

			str = push_str(p, q);

			if (is_keyword(str)) {
				tok = token_new(last = T_KEYWORD, p, q - p);
			} else if (is_plain_type(str)) {
				tok = token_new(last = T_DATATYPE, p, q - p);
			} else
				tok = token_new(last = T_IDENT, p, q - p);

			token_list_append(list, tok);
			free(str);
			p += q - p;
			continue;
		}

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

		if (ispunct(*p)) {
			token *tok;

			static char *operators[] = {"=",  "==", "!=", "+",  "-",
						    "--", "++", "-=", "+=", "/",
						    "%",  "/=", "%=", "*"};

			for (int i = 0;
			     i < LEN(operators); i++) {
				if (!strncmp(p, operators[i],
					     strlen(operators[i]))) {
					tok = token_new(last = T_OPERATOR, p,
							strlen(operators[i]));
					token_list_append(list, tok);

					p += strlen(operators[i]);
					continue;
				}
			}

			while (isspace(*p))
				p++;

			if (!ispunct(*p))
				continue;

			tok = token_new(last = T_PUNCT, p, 1);
			token_list_append(list, tok);

			p++;
			continue;
		}

		p++;
	}

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
