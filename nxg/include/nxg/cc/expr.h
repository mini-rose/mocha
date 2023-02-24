/* single expression in the AST
   Copyright (c) 2023 mini-rose */

#pragma once

typedef enum
{
	E_SKIP, /* empty expression, skip these in emit() */
	E_MODULE,
	E_FUNCTION,
	E_CALL,
	E_RETURN, /* data is a pointer to a value_expr_t */
	E_VARDECL,
	E_ASSIGN,
	E_VALUE,
	E_CONDITION,
	E_BLOCK, /* just a {} block */
} expr_type;

typedef struct expr expr_t;

struct expr
{
	expr_type type;
	expr_t *next;
	expr_t *child;
	void *data;
};
