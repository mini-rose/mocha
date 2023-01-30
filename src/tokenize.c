#include <ctype.h>
#include <memory.h>
#include <nxg/error.h>
#include <nxg/file.h>
#include <nxg/tokenize.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void token_destroy(token *tok);

void token_list_append(token_list *list, token *tok)
{
	list->tokens =
	    (token **) realloc(list->tokens, ++list->length * sizeof(token));
	list->tokens[list->length - 1] = tok;
}

token_list *token_list_new()
{
	token_list *list = (token_list *) malloc(sizeof(token_list));
	list->tokens = NULL;
	list->length = 0;
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
	free((void *) tok->value);
	free(tok);
}

token *token_new(token_t type, const char *value, int len)
{
	token *tok = (token *) malloc(sizeof(token));

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

	printf("%s: '%.*s'", tok_str[tok->type], tok->len, tok->value);
}

void token_list_print(token_list *list)
{
	puts("[");
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
	static const char *keywords[] = {"str", "bool", "i8",  "i16",
					 "i32", "i64",  "f32", "f64"};

	for (int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
		if (!strcmp(str, keywords[i]))
			return true;

	return false;
}

token_list *tokens(file *source)
{
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

			p += 2;
			continue;
		}

		if (*p == '\n') {
			token *tok = token_new(T_NEWLINE, p, 0);
			token_list_append(list, tok);
			p++;
			continue;
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

			tok = token_new(T_STRING, p, q - p);
			token_list_append(list, tok);

			p += q - p + 1;
			continue;
		}

		if (isalpha(*p)) {
			token *tok;
			char *str;
			char *q = p;

			while ((*q > 96 && *q < 123) || isdigit(*q))
				q++;

			str = push_str(p, q);

			if (is_keyword(str))
				tok = token_new(T_KEYWORD, p, q - p);
			else if (is_type(str))
				tok = token_new(T_DATATYPE, p, q - p);
			else
				tok = token_new(T_IDENT, p, q - p);

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

			tok = token_new(T_NUMBER, p, q - p);
			token_list_append(list, tok);

			p += q - p;
			continue;
		}

		if (ispunct(*p)) {
			token *tok;

			static char *operators[] = {"=",  "==", "!=", "+",  "-",
						    "--", "++", "-=", "+=", "/",
						    "%",  "/=", "%="};

			for (int i = 0;
			     i < sizeof(operators) / sizeof(*operators); i++) {
				if (!strncmp(p, operators[i],
					     strlen(operators[i]))) {
					tok = token_new(T_OPERATOR, p,
							strlen(operators[i]));
					token_list_append(list, tok);

					p += strlen(operators[i]);
					continue;
				}
			}

			if (*p == ' ') {
				p++;
				continue;
			}

			tok = token_new(T_PUNCT, p, 1);
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
	static const int siz = sizeof(toknames) / sizeof(*toknames);

	if (toktype < 0 || toktype >= siz)
		return "<tokname() failed>";
	return toknames[toktype];
}
