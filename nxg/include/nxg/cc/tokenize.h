#pragma once

#include <nxg/utils/file.h>

/* Token type */
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

/* Token */
typedef struct
{
	token_t type;
	const char *value;
	int len;
} token;

/* Dynamically allocated token list */
typedef struct
{
	file_t *source;
	token **tokens;
	int length;
	int iter;
} token_list;

/* Resturns string representation of token type */
const char *tokname(token_t toktype);

/* Tokenize file */
token_list *tokens(file_t *source);

/* Token list free */
void token_list_destroy(token_list *tok);

/* Prints token list */
void token_list_print(token_list *list);
