#pragma once

typedef enum
{
	T_OPERATOR,
	T_DATATYPE,
	T_KEYWORD,
	T_NUMBER,
	T_STRING,
	T_IDENT,
	T_PUNCT,
	T_END
} token_t;

typedef struct
{
	token_t type;
	const char *value;
} token;

typedef struct
{
	token **tokens;
	int length;
} token_list;

token_list *tokens(const char *path);
void token_list_destroy(token_list *tok);
void token_list_print(token_list *list);
