#pragma once

typedef enum
{
	OPERATOR,
	DATATYPE,
	KEYWORD,
	NUMBER,
	STRING,
	IDENT,
	PUNCT,
	END
} token_t;

typedef struct {
	token_t type;
	const char *value;
} token;

typedef struct {
	token **tokens;
	int lenght;
} token_list;

token_list *tokens(const char *path);
void token_list_destroy(token_list *tok);
