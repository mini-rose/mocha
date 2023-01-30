#pragma once
#include <nxg/tokenize.h>

typedef enum
{
	T_FLOAT,
	T_STR,
	T_TABLE,
	T_INT,
	T_NULL,
	T_BOOL,
	T_VOID
} plain_type;

typedef enum
{
	E_MODULE,
	E_FUNCTION,
	E_CALL,
	E_VARDECL,
	E_ASSIGN,
} expr_type;

/* top-level module */
typedef struct
{
	char *name;
} mod_expr_t;

/* function declaration */
typedef struct
{
	plain_type **args;
	plain_type returns;
	char *name;
} fn_expr_t;

typedef void (*expr_free_handle)(void *expr);
typedef struct expr expr_t;

struct expr
{
	expr_type type;
	expr_t *next;
	expr_t *child;
	expr_free_handle data_free;
	void *data;
};

#define E_AS_MOD(DATAPTR) ((mod_expr_t *) (DATAPTR))
#define E_AS_FN(DATAPTR)  ((fn_expr_t *) (DATAPTR))

expr_t *parse(file *source, token_list *list);
void expr_destroy(expr_t *expr);
