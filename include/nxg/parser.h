#pragma once
#include <nxg/tokenize.h>

typedef enum
{
	T_NULL,
	T_STR,
	T_I8,
	T_I16,
	T_I32,
	T_I64,
	T_I128,
	T_F32,
	T_F64,
	T_BOOL,
	T_VOID
} plain_type;

typedef enum
{
	E_MODULE,
	E_FUNCTION,
	E_CALL,
	E_VARDEF,
	E_VARDECL,
	E_ASSIGN,
} expr_type;

/* top-level module */
typedef struct
{
	char *name;
} mod_expr_t;

typedef struct {
	char *name;
	plain_type type;
} arg_t;

/* function definition */
typedef struct
{
	int n_args;
	arg_t **args;
	char *name;
	plain_type return_type;
} fn_expr_t;

/* variable declaration */
typedef struct
{
	plain_type type;
	char *name;
} var_decl_expr_t;

/* variable definition */
typedef struct
{
	plain_type type;
	char *name;
	char *value;
} var_expr_t;

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

expr_t *parse(token_list *list);
void expr_destroy(expr_t *expr);
