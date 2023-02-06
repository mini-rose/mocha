#pragma once

#include <nxg/utils/file.h>

typedef enum
{
	T_OPERATOR = 0,
	T_DATATYPE = 1,
	T_NEWLINE = 2,
	T_KEYWORD = 3,
	T_NUMBER = 4,
	T_STRING = 5,
	T_IDENT = 6,
	T_PUNCT = 7,
	T_END = 8
} token_t;

typedef struct
{
	token_t type;
	const char *value;
	int len;
} token;

typedef struct
{
	file *source;
	token **tokens;
	int length;
	int iter;
} token_list;

const char *tokname(token_t toktype);
token_list *tokens(file *source);
void token_list_destroy(token_list *tok);
void token_list_print(token_list *list);
