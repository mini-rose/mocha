#pragma once
#include <tokenize.h>

typedef enum
{
	T_FLOAT,
	T_STR,
	T_TABLE,
	T_INT,
	T_NULL,
	T_BOOL,
	T_VOID
} types;

typedef enum
{
	FUNCTION,
	FUNCTION_CALL,
} expr_t;

typedef struct
{
	types **args;
	types returns;
} fn_t;

typedef struct expr expr;
struct expr
{
	expr *next;
	expr_t type;
};

expr *parse(token_list *list);
