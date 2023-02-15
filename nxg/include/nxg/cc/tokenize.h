#pragma once

#include <nxg/utils/file.h>

/* Token type */
typedef enum
{
	T_DATATYPE,
	T_NEWLINE,
	T_KEYWORD,
	T_NUMBER,
	T_STRING,
	T_IDENT,
	T_PUNCT,
	T_END,

	/* Operator types */
	T_EQ,    // ==
	T_NEQ,   // !=
	T_ASS,   // =
	T_ADD,   // +
	T_ADDA,  // +=
	T_ARROW, // ->
	T_DEC,   // --
	T_INC,   // ++
	T_SUBA,  // -=
	T_DIV,   // /
	T_MOD,   // %
	T_DIVA,  // /=
	T_MODA,  // %=
	T_MUL,   // *
	T_SUB    // -
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
