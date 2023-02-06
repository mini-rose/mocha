#pragma once
#include <nxg/tokenize.h>
#include <nxg/type.h>
#include <stdbool.h>
#include <stddef.h>

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
} expr_type;

typedef void (*expr_free_handle)(void *expr);
typedef struct expr expr_t;
typedef struct fn_expr fn_expr_t;
typedef struct literal_expr literal_expr_t;
typedef struct call_expr call_expr_t;
typedef struct value_expr value_expr_t;

struct expr
{
	expr_type type;
	expr_t *next;
	expr_t *child;
	expr_free_handle data_free;
	void *data;
};

/* top-level module */
typedef struct
{
	char *name;
	char *source_name;
	fn_expr_t **decls;
	int n_decls;
} mod_expr_t;

typedef enum
{
	VE_NULL, /* */
	VE_REF,  /* name */
	VE_LIT,  /* literal */
	VE_CALL, /* call */
	VE_ADD,  /* left + right */
	VE_SUB,  /* left - right */
	VE_MUL,  /* left * right */
	VE_DIV,  /* left / right */
} value_expr_type;

struct value_expr
{
	plain_type return_type;
	value_expr_type type;
	union
	{
		char *name;
		literal_expr_t *literal;
		call_expr_t *call;
		value_expr_t *left;
	};
	value_expr_t *right;
};

/* variable declaration */
typedef struct
{
	plain_type type;
	char *name;
} var_decl_expr_t;

#define FN_NOMANGLE 1

/* function definition */
struct fn_expr
{
	char *name;
	var_decl_expr_t **params;
	var_decl_expr_t **locals;
	int n_params;
	int n_locals;
	plain_type return_type;
	int flags;
};

/* variable assignment */
typedef struct
{
	char *name;
	value_expr_t *value;
} assign_expr_t;

struct call_expr
{
	char *name;
	int n_args;
	value_expr_t **args;
	fn_expr_t *func;
};

typedef struct
{
	char *ptr;
	int len;
} sized_string_t;

struct literal_expr
{
	plain_type type;
	union
	{
		int v_i32;
		float v_f32;
		sized_string_t v_str;
	};
};

typedef struct
{
	plain_type type;
	char *value;
} num_expr_t;

#define E_AS_MOD(DATAPTR)   ((mod_expr_t *) (DATAPTR))
#define E_AS_FN(DATAPTR)    ((fn_expr_t *) (DATAPTR))
#define E_AS_VDECL(DATAPTR) ((var_decl_expr_t *) (DATAPTR))
#define E_AS_ASS(DATAPTR)   ((assign_expr_t *) (DATAPTR))
#define E_AS_LIT(DATAPTR)   ((literal_expr_t *) DATAPTR)
#define E_AS_VAL(DATAPTR)   ((value_expr_t *) DATAPTR)
#define E_AS_CALL(DATAPTR)  ((call_expr_t *) DATAPTR)

expr_t *parse(token_list *list, const char *module_id);
void expr_destroy(expr_t *expr);
void expr_print(expr_t *expr);

void literal_default(literal_expr_t *literal);
char *stringify_literal(literal_expr_t *literal);

const char *value_expr_type_name(value_expr_type t);
value_expr_type value_expr_type_from_op(token *op);

var_decl_expr_t *node_resolve_local(expr_t *node, const char *name, int len);
bool node_has_named_local(expr_t *node, const char *name, int len);

void fn_add_param(fn_expr_t *fn, const char *name, int len, plain_type type);
char *fn_str_signature(fn_expr_t *func);
