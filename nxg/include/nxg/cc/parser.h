#pragma once
#include <nxg/cc/tokenize.h>
#include <nxg/cc/type.h>
#include <nxg/nxg.h>
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

typedef enum
{
	MO_LOCAL,
	MO_STD
} mod_origin;

/* top-level module */
typedef struct
{
	char *name;
	char *source_name;
	mod_origin origin;
	fn_expr_t **decls;
	int n_decls;
	fn_expr_t **local_decls;
	int n_local_decls;
	expr_t **imported;
	int n_imported;
	char **c_objects;
	int n_c_objects;
} mod_expr_t;

typedef enum
{
	VE_NULL,  /* */
	VE_REF,   /* name */
	VE_LIT,   /* literal */
	VE_CALL,  /* call */
	VE_ADD,   /* left + right */
	VE_SUB,   /* left - right */
	VE_MUL,   /* left * right */
	VE_DIV,   /* left / right */
	VE_PTR,   /* &name */
	VE_DEREF, /* *name */
} value_expr_type;

struct value_expr
{
	type_t *return_type;
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
	type_t *type;
	char *name;
	bool used;
	token *decl_location;
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
	type_t *return_type;
	int flags;
	bool emitted;
};

/* variable assignment */
typedef struct
{
	value_expr_t *to;
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
	type_t *type;
	union
	{
		int v_i32;
		float v_f32;
		sized_string_t v_str;
	};
};

#define E_AS_MOD(DATAPTR)   ((mod_expr_t *) (DATAPTR))
#define E_AS_FN(DATAPTR)    ((fn_expr_t *) (DATAPTR))
#define E_AS_VDECL(DATAPTR) ((var_decl_expr_t *) (DATAPTR))
#define E_AS_ASS(DATAPTR)   ((assign_expr_t *) (DATAPTR))
#define E_AS_LIT(DATAPTR)   ((literal_expr_t *) DATAPTR)
#define E_AS_VAL(DATAPTR)   ((value_expr_t *) DATAPTR)
#define E_AS_CALL(DATAPTR)  ((call_expr_t *) DATAPTR)

expr_t *parse(settings_t *settings, token_list *list, const char *module_id);
void expr_destroy(expr_t *expr);
void expr_print(expr_t *expr);
const char *expr_typename(expr_type type);

void literal_default(literal_expr_t *literal);
char *stringify_literal(literal_expr_t *literal);

const char *value_expr_type_name(value_expr_type t);
value_expr_type value_expr_type_from_op(token *op);

var_decl_expr_t *node_resolve_local(expr_t *node, const char *name, int len);
bool node_has_named_local(expr_t *node, const char *name, int len);

bool fn_sigcmp(fn_expr_t *first, fn_expr_t *other);
void fn_add_param(fn_expr_t *fn, const char *name, int len, type_t *type);
char *fn_str_signature(fn_expr_t *func, bool with_colors);
