#include <ctype.h>
#include <error.h>
#include <file.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tokenize.h>

static void token_destroy(token *tok);

void token_list_append(token_list *list, token *tok)
{
	list->tokens =
		(token **) realloc(list->tokens, ++list->lenght * sizeof(token));
	list->tokens[list->lenght - 1] = tok;
}

token_list *token_list_new()
{
	token_list *list = (token_list *) malloc(sizeof(token_list));
	list->tokens = NULL;
	list->lenght = 0;
	return list;
}

void token_list_destroy(token_list *list)
{
	for (int i = 0; i < list->lenght; i++)
		token_destroy(list->tokens[i]);

	free(list->tokens);
	free(list);
}

void token_destroy(token *tok)
{
	free((void *) tok->value);
	free(tok);
}

token *token_new(token_t type, const char *value)
{
	token *tok = (token *) malloc(sizeof(token));

	tok->value = (const char *) calloc(1, strlen(value) * sizeof(char) + 1);
	tok->type = type;
	strcpy((char *) tok->value, value);

	return tok;
}

void token_print(token *tok)
{
	static const char *tok_str[] = {"OPERATOR", "DATATYPE", "KEYWORD",
					"NUMBER",   "STRING",   "IDENT",
					"PUNCT",    "END"};

	printf("%s: '%s'", tok_str[tok->type], tok->value);
}

void token_list_print(token_list *list)
{
	puts("[");
	for (int i = 0; i < list->lenght; i++) {
		fputs("\t{", stdout);
		token_print(list->tokens[i]);
		fputs("}", stdout);

		if (i + 1 != list->lenght)
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

static bool is_keyword(const char *str)
{
	static const char *keywords[] = {"fn",  "ret",   "if",  "elif", "else",
					 "for", "while", "and", "or",   "not"};

	for (int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
		if (!strcmp(str, keywords[i]))
			return true;

	return false;
}

static bool is_type(const char *str)
{
	static const char *keywords[] = {"int", "str", "float", "array"};

	for (int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
		if (!strcmp(str, keywords[i]))
			return true;

	return false;
}

token_list *tokens(const char *path)
{
	token_list *list = token_list_new();
	file *f = file_new(path);

	char *p = f->content;

	while (*p) {
		if (!strncmp(p, "/*", 2)) {
			char *q = strstr(p + 2, "*/");

			if (!q)
				error_at(f->content, p, "Unterminated comment.");

			p += 2;
			continue;
		}

		if (isspace(*p) || *p == '\n') {
			p++;
			continue;
		}

		if (*p == '\'' || *p == '"') {
			char *str;
			token *tok;
			char *q = strend(p++);

			if (!q)
				error_at(f->content, p - 1, "Unterminated quoted string.");

			str = push_str(p, q);
			tok = token_new(STRING, str);
			token_list_append(list, tok);
			free(str);

			p += q - p + 1;
			continue;
		}

		if (isalpha(*p)) {
			token *tok;
			char *str;
			char *q = p;

			while (*q > 96 && *q < 123)
				q++;

			str = push_str(p, q);

			if (is_keyword(str))
				tok = token_new(KEYWORD, str);
			else if (is_type(str))
				tok = token_new(DATATYPE, str);
			else
				tok = token_new(IDENT, str);

			token_list_append(list, tok);
			free(str);
			p += q - p;
			continue;
		}

		if (isdigit(*p) || (*p == '.' && isdigit(*(p + 1)))) {
			token *tok;
			char *str;
			char *q = p;

			if (*q == '.')
				q++;

			while (isdigit(*q) || *q == '.')
				q++;

			str = push_str(p, q);
			tok = token_new(NUMBER, str);
			token_list_append(list, tok);
			free(str);

			p += q - p;
			continue;
		}

		if (ispunct(*p)) {
			token *tok;

			static char *operators[] = {"=",  "==", "!=", "+",  "-",
						    "--", "++", "-=", "+=", "/",
						    "%",  "/=", "%="};

			for (int i = 0; i < sizeof(operators) / sizeof(*operators);
				 i++) {
				if (!strncmp(p, operators[i], strlen(operators[i]))) {
					tok = token_new(OPERATOR, operators[i]);
					token_list_append(list, tok);

					p += strlen(operators[i]);
					continue;
				}
			}

			char str[] = {*p, 0};

			if (*p == ' ') {
				p++;
				continue;
			}

			tok = token_new(PUNCT, str);
			token_list_append(list, tok);

			p++;
			continue;
		}

		p++;
	}

	token *tok = token_new(END, "");
	token_list_append(list, tok);

	file_destroy(f);
	return list;
}
