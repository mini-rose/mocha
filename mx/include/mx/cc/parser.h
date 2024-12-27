/* parser.h - mocha tokens to AST parser
   Copyright (c) 2023 mini-rose */

#pragma once

#include <mx/cc/expr.h>
#include <mx/cc/tokenize.h>
#include <mx/cc/type.h>
#include <mx/mx.h>
#include <mx/utils/error.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct fn_expr fn_expr_t;
typedef struct literal_expr literal_expr_t;
typedef struct call_expr call_expr_t;
typedef struct value_expr value_expr_t;

typedef enum
{
	MO_LOCAL,
	MO_STD
} mod_origin;

typedef struct
{
	expr_t **modules;
	int n_modules;
} std_modules_t;

typedef struct
{
	char **objects;
	int n;
} c_objects_t;

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
	type_t **type_decls;
	int n_type_decls;
	std_modules_t *std_modules;
	c_objects_t *c_objects;
} mod_expr_t;

typedef enum
{
	VE_NULL,   /* */
	VE_REF,    /* name */
	VE_LIT,    /* literal */
	VE_CALL,   /* call */
	VE_ADD,    /* left + right */
	VE_SUB,    /* left - right */
	VE_MUL,    /* left * right */
	VE_DIV,    /* left / right */
	VE_PTR,    /* &name */
	VE_DEREF,  /* *name */
	VE_MREF,   /* name.member */
	VE_MPTR,   /* &name.member */
	VE_MDEREF, /* *name.member */
	VE_EQ,     /* left == right */
	VE_NEQ,    /* left != right */
	VE_TUPLE,  /* tuple */
} value_expr_type;

typedef struct
{
	int len;
	type_t *element_type;
	value_expr_t **values;
} tuple_expr_t;

struct value_expr
{
	type_t *return_type;
	value_expr_type type;
	char *member;
	bool force_precendence;
	union
	{
		char *name;
		literal_expr_t *literal;
		call_expr_t *call;
		value_expr_t *left;
		tuple_expr_t *tuple;
	};
	value_expr_t *right;
};

/* variable declaration */
typedef struct
{
	type_t *type;
	char *name;
	bool used;
	bool used_by_emit;
	token *decl_location;
} var_decl_expr_t;

typedef struct
{
	value_expr_t *cond;
	expr_t *if_block;
	expr_t *else_block;
} condition_expr_t;

typedef struct
{
	var_decl_expr_t **locals;
	int n_locals;
	expr_t *parent;
} block_expr_t;

#define FN_NOMANGLE 1

/* function definition */
struct fn_expr
{
	char *name;
	expr_t *module;
	object_type_t *object; /* method if non-null */
	bool is_static;
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
	char *object_name;
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
		bool v_bool;
		unsigned char v_i8;
		short v_i16;
		int v_i32;
		long v_i64;
		float v_f32;
		double v_f64;
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
#define E_AS_COND(DATAPTR)  ((condition_expr_t *) DATAPTR)
#define E_AS_BLOCK(DATAPTR) ((block_expr_t *) DATAPTR)

expr_t *parse(expr_t *parent, expr_t *module, settings_t *settings,
	      token_list *list, const char *module_id);
void expr_print(expr_t *expr);
void expr_print_value_expr(value_expr_t *val, int level);
const char *expr_typename(expr_type type);

void literal_default(literal_expr_t *literal);
char *stringify_literal(literal_expr_t *literal);

const char *value_expr_type_name(value_expr_type t);
value_expr_type value_expr_type_from_op(token_list *tokens, token *op);

var_decl_expr_t *node_resolve_local(expr_t *node, const char *name, int len);
bool node_has_named_local(expr_t *node, const char *name, int len);

bool fn_sigcmp(fn_expr_t *first, fn_expr_t *other);
void fn_add_param(fn_expr_t *fn, const char *name, int len, type_t *type);
char *fn_str_signature(fn_expr_t *func, bool with_colors);

token *index_tok(token_list *list, int index);
token *next_tok(token_list *list);
token *after_tok(token_list *list, token *from);
token *before_tok(token_list *list, token *from);

bool is_var_decl(token_list *tokens, token *tok);
bool is_type(token_list *tokens, token *tok);
bool is_call(token_list *tokens, token *tok);
bool is_member_call(token_list *tokens, token *tok);
bool is_literal(token *tok);
bool is_symbol(token *tok);
bool is_member(token_list *tokens, token *tok);
bool is_pointer_to_member(token_list *tokens, token *tok);
bool is_dereference(token_list *tokens, token *tok);
bool is_pointer_to(token_list *tokens, token *tok);
bool is_rvalue(token_list *tokens, token *tok);
bool is_operator(token *tok);
bool is_comparison(token_list *tokens, token *tok);
bool is_builtin_function(token *name);
bool is_integer(token *tok);
bool is_float(token *tok);
bool is_var_assign(token_list *tokens, token *tok);

int call_token_len(token_list *tokens, token *tok);
int rvalue_token_len(token_list *tokens, token *tok);
int type_token_len(token_list *tokens, token *tok);

err_t parse_builtin_call(expr_t *parent, expr_t *mod, token_list *tokens,
			 token *tok);
err_t parse_inline_call(settings_t *settings, expr_t *parent, expr_t *mod,
			call_expr_t *data, token_list *tokens, token *tok);

value_expr_t *parse_value_expr(settings_t *settings, expr_t *context,
			       expr_t *mod, value_expr_t *node,
			       token_list *tokens, token *tok);

void parse_call(settings_t *settings, expr_t *parent, expr_t *mod,
		token_list *tokens, token *tok);
void parse_member_call(settings_t *settings, expr_t *parent, expr_t *mod,
		       token_list *tokens, token *tok);
type_t *parse_type(expr_t *context, token_list *tokens, token *tok);
void parse_literal(value_expr_t *node, token_list *tokens, token *tok);
expr_t *expr_add_child(expr_t *parent);

bool convert_int_value(value_expr_t *value, type_t *into_type);

typedef struct
{
	const char *value;
	int len;
} highlight_t;

highlight_t highlight_value(token_list *tokens, token *tok);

#define TOK_IS(TOK, TYPE, VALUE)                                               \
 (((TOK)->type == (TYPE)) && !strncmp((TOK)->value, VALUE, (TOK)->len))
