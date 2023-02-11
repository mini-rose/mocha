#include <ctype.h>
#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
#include <nxg/utils/utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TOK_IS(TOK, TYPE, VALUE)                                               \
 (((TOK)->type == (TYPE)) && !strncmp((TOK)->value, VALUE, (TOK)->len))

static void fn_expr_free(fn_expr_t *function);
static void value_expr_free(value_expr_t *value);
static void call_expr_free(call_expr_t *call);
static void expr_print_value_expr(value_expr_t *val, int level);

static void mod_expr_free(mod_expr_t *module)
{
	for (int i = 0; i < module->n_decls; i++) {
		fn_expr_free(module->decls[i]);
		free(module->decls[i]);
	}

	for (int i = 0; i < module->n_imported; i++)
		expr_destroy(module->imported[i]);

	for (int i = 0; i < module->n_imported; i++)
		free(module->c_objects[i]);

	free(module->c_objects);
	free(module->imported);
	free(module->name);
	free(module->source_name);
	free(module->local_decls);
	free(module->decls);
}

static void fn_expr_free(fn_expr_t *function)
{
	int i;

	free(function->name);

	for (i = 0; i < function->n_params; i++) {
		type_destroy(function->params[i]->type);
		free(function->params[i]->name);
		free(function->params[i]);
	}

	type_destroy(function->return_type);
	free(function->locals);
	free(function->params);
}

static void literal_expr_free(literal_expr_t *lit)
{
	if (lit->type->type != TY_PLAIN)
		error("a literal cannot be a non-plain type");

	if (lit->type->v_plain == PT_STR)
		free(lit->v_str.ptr);

	type_destroy(lit->type);
}

static void value_expr_free(value_expr_t *value)
{
	if (!value)
		return;

	if (value->type == VE_NULL)
		return;
	else if (value->type == VE_REF || value->type == VE_PTR
		 || value->type == VE_DEREF) {
		free(value->name);
	} else if (value->type == VE_LIT) {
		literal_expr_free(value->literal);
		free(value->literal);
	} else if (value->type == VE_CALL) {
		call_expr_free(value->call);
		free(value->call);
	} else {
		value_expr_free(value->left);
		free(value->left);
		value_expr_free(value->right);
		free(value->right);
	}

	type_destroy(value->return_type);
}

static void call_expr_free(call_expr_t *call)
{
	free(call->name);
	for (int i = 0; i < call->n_args; i++) {
		value_expr_free(call->args[i]);
		free(call->args[i]);
	}
	free(call->args);
}

static void assign_expr_free(assign_expr_t *assign)
{
	value_expr_free(assign->value);
	value_expr_free(assign->to);
	free(assign->to);
	free(assign->value);
}

static void var_decl_expr_free(var_decl_expr_t *variable)
{
	type_destroy(variable->type);
	free(variable->name);
}

static token *index_tok(token_list *list, int index)
{
	static token end_token = {.type = T_END, .value = "", .len = 0};

	if (list->iter >= list->length)
		return &end_token;
	return list->tokens[index - 1];
}

static token *next_tok(token_list *list)
{
	return index_tok(list, list->iter++);
}

static expr_t *expr_add_next(expr_t *expr)
{
	expr_t *node = calloc(sizeof(*node), 1);

	while (expr->next)
		expr = expr->next;

	expr->next = node;
	return node;
}

static expr_t *expr_add_child(expr_t *parent)
{
	if (parent->child)
		return expr_add_next(parent->child);

	expr_t *node = calloc(sizeof(*node), 1);
	parent->child = node;
	return node;
}

bool fn_sigcmp(fn_expr_t *first, fn_expr_t *other)
{
	if (strcmp(first->name, other->name))
		return false;
	if (first->n_params != other->n_params)
		return false;

	for (int i = 0; i < first->n_params; i++) {
		if (!type_cmp(first->params[i]->type, other->params[i]->type))
			return false;
	}

	return true;
}

void fn_add_param(fn_expr_t *fn, const char *name, int len, type_t *type)
{
	fn->params =
	    realloc(fn->params, sizeof(var_decl_expr_t) * ++fn->n_params);
	fn->params[fn->n_params - 1] = malloc(sizeof(var_decl_expr_t));
	fn->params[fn->n_params - 1]->name = strndup(name, len);
	fn->params[fn->n_params - 1]->type = type_copy(type);
}

var_decl_expr_t *node_resolve_local_touch(expr_t *node, const char *name,
					  int len, bool touch)
{
	if (len == 0)
		len = strlen(name);

	/* only support functions now */
	if (node->type != E_FUNCTION)
		return NULL;

	fn_expr_t *fn = node->data;

	for (int i = 0; i < fn->n_params; i++) {
		if (!strncmp(fn->params[i]->name, name, len)) {
			if (touch)
				fn->params[i]->used = true;
			return fn->params[i];
		}
	}

	for (int i = 0; i < fn->n_locals; i++) {
		if (!strncmp(fn->locals[i]->name, name, len)) {
			if (touch)
				fn->locals[i]->used = true;
			return fn->locals[i];
		}
	}

	return NULL;
}

var_decl_expr_t *node_resolve_local(expr_t *node, const char *name, int len)
{
	return node_resolve_local_touch(node, name, len, true);
}

bool node_has_named_local(expr_t *node, const char *name, int len)
{
	return node_resolve_local_touch(node, name, len, false) != NULL;
}

/**
 * name: type
 */
static bool is_var_decl(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter);
	if (!TOK_IS(tok, T_PUNCT, ":"))
		return false;

	tok = index_tok(tokens, tokens->iter + 1);
	if (tok->type != T_DATATYPE)
		return false;

	return true;
}

/**
 * ident([arg, ...])
 */
static bool is_call(token_list *tokens, token *tok)
{
	int index = tokens->iter;

	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, index);
	if (!TOK_IS(tok, T_PUNCT, "("))
		return false;

	/* find the closing brace */
	do {
		tok = index_tok(tokens, ++index);
		if (TOK_IS(tok, T_PUNCT, ")"))
			return true;
	} while (tok->type != T_END && tok->type != T_NEWLINE);

	return false;
}

/**
 * literal ::= string | integer | float | "null"
 */
static bool is_literal(token *tok)
{
	return tok->type == T_NUMBER || tok->type == T_STRING
	    || TOK_IS(tok, T_DATATYPE, "null");
}

static bool is_reference(token *tok)
{
	return tok->type == T_IDENT;
}

/**
 * dereference ::= *ident
 */
static bool is_dereference(token_list *tokens, token *tok)
{
	if (!TOK_IS(tok, T_OPERATOR, "*") && !TOK_IS(tok, T_PUNCT, "*"))
		return false;

	tok = index_tok(tokens, tokens->iter);
	if (tok->type == T_IDENT)
		return true;

	return false;
}

/**
 * pointer-to ::= &ident
 */
static bool is_pointer_to(token_list *tokens, token *tok)
{
	if (!TOK_IS(tok, T_PUNCT, "&"))
		return false;

	tok = index_tok(tokens, tokens->iter);
	if (tok->type == T_IDENT)
		return true;

	return false;
}

/*
 * value ::= literal
 *       ::= ident
 *       ::= call
 *       ::= dereference
 *       ::= pointer-to
 */
static bool is_single_value(token_list *tokens, token *tok)
{
	return is_literal(tok) || is_reference(tok) || is_call(tokens, tok)
	    || is_dereference(tokens, tok) || is_pointer_to(tokens, tok);
}

static bool is_integer(token *tok)
{
	for (int i = 0; i < tok->len; i++) {
		if (!isdigit(tok->value[i]))
			return false;
	}

	return true;
}

static bool is_float(token *tok)
{
	bool found_dot = false;
	for (int i = 0; i < tok->len; i++) {
		if (tok->value[i] == '.') {
			if (!found_dot)
				found_dot = true;
			else
				return false;
			continue;
		}

		if (!isdigit(tok->value[i]))
			return false;
	}

	return true;
}

static void parse_string_literal(sized_string_t *val, token *tok)
{
	char *buf = calloc(tok->len + 1, 1);
	int j = 0;

	for (int i = 0; i < tok->len; i++) {
		if (!strncmp(&tok->value[i], "\\n", 2)) {
			buf[j++] = '\n';
			i++;
		} else {
			buf[j++] = tok->value[i];
		}
	}

	val->len = j;
	val->ptr = buf;
}

static void parse_literal(value_expr_t *node, token_list *tokens, token *tok)
{
	node->type = VE_LIT;

	/* string */
	if (tok->type == T_STRING) {
		node->literal = calloc(1, sizeof(*node->literal));
		node->return_type = type_new_plain(PT_STR);
		node->literal->type = type_new_plain(PT_STR);
		parse_string_literal(&node->literal->v_str, tok);
		return;
	}

	if (tok->type == T_NUMBER) {
		node->literal = calloc(1, sizeof(*node->literal));
		char *tmp = strndup(tok->value, tok->len);

		if (is_integer(tok)) {
			node->return_type = type_new_plain(PT_I32);
			node->literal->type = type_new_plain(PT_I32);
			node->literal->v_i32 = strtol(tmp, NULL, 10);
		} else if (is_float(tok)) {
			node->return_type = type_new_plain(PT_F32);
			node->literal->type = type_new_plain(PT_F32);
			node->literal->v_f32 = strtof(tmp, NULL);
		} else {
			error_at(tokens->source, tok->value, tok->len,
				 "cannot parse this number");
		}

		free(tmp);

		return;
	}

	/* "null" */
	if (TOK_IS(tok, T_DATATYPE, "null")) {
		node->return_type = type_new_null();
		node->type = VE_NULL;
		return;
	}

	error_at(tokens->source, tok->value, tok->len,
		 "unparsable literal `%.*s`", tok->len, tok->value);
}

static void parse_reference(value_expr_t *node, expr_t *context,
			    token_list *tokens, token *tok)
{
	if (!node_has_named_local(context, tok->value, tok->len)) {
		error_at(tokens->source, tok->value, tok->len,
			 "use of undeclared variable");
	}

	node->type = VE_REF;
	node->name = strndup(tok->value, tok->len);

	var_decl_expr_t *var =
	    node_resolve_local(context, tok->value, tok->len);
	node->return_type = type_copy(var->type);
}

static type_t *parse_type(token_list *tokens, token *tok)
{
	type_t *ty;

	if (TOK_IS(tok, T_PUNCT, "&")) {
		tok = next_tok(tokens);
		ty = type_new_null();
		ty->type = TY_POINTER;
		ty->v_base = parse_type(tokens, tok);
		return ty;
	}

	if (tok->type == T_IDENT) {
		error_at(tokens->source, tok->value, tok->len,
			 "object types are not yet implemented");
	}

	if (tok->type == T_DATATYPE) {
		ty = type_from_sized_string(tok->value, tok->len);
	}

	tok = index_tok(tokens, tokens->iter);
	if (TOK_IS(tok, T_PUNCT, "[")) {
		/* array type */
		tok = next_tok(tokens);
		tok = next_tok(tokens);

		if (tok->type != T_NUMBER) {
			error_at(tokens->source, tok->value, tok->len,
				 "missing array size");
		}

		type_t *array_ty = type_new_null();
		array_ty->type = TY_ARRAY;
		array_ty->len = strtol(tok->value, NULL, 10);
		array_ty->v_base = ty;

		tok = next_tok(tokens);
		if (!TOK_IS(tok, T_PUNCT, "]")) {
			error_at(tokens->source, tok->value, tok->len,
				 "expected closing bracket `]`");
		}

		return array_ty;
	} else {
		/* regular type */
		return ty;
	}

	return type_new_null();
}

static value_expr_t *call_add_arg(call_expr_t *call)
{
	value_expr_t *node = calloc(1, sizeof(*node));
	call->args =
	    realloc(call->args, sizeof(value_expr_t *) * (call->n_args + 1));
	call->args[call->n_args++] = node;
	return node;
}

static bool is_builtin_function(token *name)
{
	static const char *builtins[] = {"__builtin_decl",
					 "__builtin_externcall"};

	for (int i = 0; i < LEN(builtins); i++) {
		if (!strncmp(builtins[i], name->value, name->len))
			return true;
	}

	return false;
}

static void collect_builtin_decl_arguments(fn_expr_t *decl, token_list *tokens)

{
	token *arg;

	while ((arg = next_tok(tokens))->type != T_NEWLINE) {
		type_t *ty = parse_type(tokens, arg);
		fn_add_param(decl, "_", 1, ty);
		type_destroy(ty);

		arg = next_tok(tokens);
		if (TOK_IS(arg, T_PUNCT, ")"))
			break;

		if (!TOK_IS(arg, T_PUNCT, ",")) {
			error_at_with_fix(tokens->source, arg->value, arg->len,
					  ",",
					  "expected comma between arguments",
					  arg->len, arg->value);
		}
	}
}

static err_t parse_builtin_call(expr_t *parent, expr_t *mod, token_list *tokens,
				token *tok)
{
	/* Built-in calls are not always function calls, they may be
	   "macro-like" functions which turn into something else. */

	token *name, *arg;

	name = tok;

	if (!strncmp("__builtin_decl", name->value, name->len)) {
		fn_expr_t *decl = module_add_decl(mod);

		decl->flags = FN_NOMANGLE;

		/* function name */
		arg = next_tok(tokens);
		arg = next_tok(tokens);
		if (arg->type != T_STRING) {
			error_at(tokens->source, arg->value, arg->len,
				 "first argument to __builtin_decl must be a "
				 "string with the function name");
		}

		decl->name = strndup(arg->value, arg->len);

		arg = next_tok(tokens);
		if (!TOK_IS(arg, T_PUNCT, ",")) {
			error_at(tokens->source, arg->value, arg->len,
				 "missing return type argument");
		}

		/* return type */
		arg = next_tok(tokens);
		if (arg->type != T_DATATYPE) {
			error_at(tokens->source, arg->value, arg->len,
				 "second argument to __builtin_decl is "
				 "expected to be the return type");
		}

		decl->return_type = parse_type(tokens, arg);

		arg = next_tok(tokens);
		if (TOK_IS(arg, T_PUNCT, ")"))
			return ERR_WAS_BUILTIN;

		if (!TOK_IS(arg, T_PUNCT, ",")) {
			error_at_with_fix(tokens->source, arg->value, arg->len,
					  ",",
					  "expected comma between arguments");
		}

		collect_builtin_decl_arguments(decl, tokens);

		return ERR_WAS_BUILTIN;
	} else {
		error_at(tokens->source, tok->value, tok->len,
			 "this builtin call is not yet implemented");
	}

	return ERR_OK;
}

typedef struct
{
	token **tokens;
	int n_tokens;
} arg_tokens_t;

static void add_arg_token(arg_tokens_t *tokens, token *tok)
{
	tokens->tokens =
	    realloc(tokens->tokens, sizeof(token *) * (tokens->n_tokens + 1));
	tokens->tokens[tokens->n_tokens++] = tok;
}

static err_t parse_inline_call(expr_t *parent, expr_t *mod, call_expr_t *data,
			       token_list *tokens, token *tok)
{
	arg_tokens_t arg_tokens = {0};
	token *fn_name_tok;
	value_expr_t *arg;

	if (is_builtin_function(tok))
		return parse_builtin_call(parent, mod, tokens, tok);

	data->name = strndup(tok->value, tok->len);
	fn_name_tok = tok;

	tok = next_tok(tokens);
	tok = next_tok(tokens);

	/* Arguments - currently only support variable names & literals
	 */
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		if (tok->type == T_IDENT || TOK_IS(tok, T_PUNCT, "&")
		    || TOK_IS(tok, T_OPERATOR, "*")) {

			arg = call_add_arg(data);
			arg->type = VE_REF;

			if (TOK_IS(tok, T_PUNCT, "&")) {
				arg->type = VE_PTR;
				tok = next_tok(tokens);

				if (is_literal(tok)) {
					error_at(
					    tokens->source, tok->value,
					    tok->len,
					    "cannot take address of literal");
				}

				if (tok->type != T_IDENT) {
					error_at(tokens->source, tok->value,
						 tok->len,
						 "expected variable name after "
						 "reference marker");
				}
			}

			if (TOK_IS(tok, T_OPERATOR, "*")) {
				arg->type = VE_DEREF;
				tok = next_tok(tokens);
				if (tok->type != T_IDENT) {
					error_at(tokens->source, tok->value,
						 tok->len,
						 "expected variable name after "
						 "dereference marker");
				}
			}

			arg->name = strndup(tok->value, tok->len);
			add_arg_token(&arg_tokens, tok);

			var_decl_expr_t *var;

			var = node_resolve_local(parent, arg->name, 0);
			if (!var) {
				error_at(tokens->source, tok->value, tok->len,
					 "use of undeclared variable");
			}

			/* if we have a reference marker (&x), then the return
			   type is the pointer type of the value */
			if (arg->type == VE_PTR)
				arg->return_type = type_pointer_of(var->type);
			else if (arg->type == VE_DEREF)
				arg->return_type = type_copy(var->type->v_base);
			else
				arg->return_type = type_copy(var->type);

		} else if (is_literal(tok)) {
			arg = call_add_arg(data);
			add_arg_token(&arg_tokens, tok);
			parse_literal(arg, tokens, tok);
		} else {
			error_at(tokens->source, tok->value, tok->len,
				 "expected value or variable name, got "
				 "`%.*s`",
				 tok->len, tok->value);
		}

		tok = next_tok(tokens);

		if (TOK_IS(tok, T_PUNCT, ")"))
			break;

		if (!TOK_IS(tok, T_PUNCT, ",")) {
			error_at_with_fix(
			    tokens->source, tok->value, tok->len, ",",
			    "expected comma seperating the arguments");
		}

		tok = next_tok(tokens);
	}

	fn_candidates_t *resolved = module_find_fn_candidates(mod, data->name);

	if (!resolved->n_candidates) {
		error_at(tokens->source, fn_name_tok->value, fn_name_tok->len,
			 "no function named `%s` found", data->name);
	}

	bool try_next;

	/* Find the matching candidate */
	for (int i = 0; i < resolved->n_candidates; i++) {
		fn_expr_t *match = resolved->candidate[i];
		try_next = false;

		if (match->n_params != data->n_args)
			continue;

		/* Check for argument types */
		for (int j = 0; j < match->n_params; j++) {
			if (!type_cmp(match->params[j]->type,
				      data->args[j]->return_type)) {
				try_next = true;
				break;
			}
		}

		if (try_next)
			continue;

		/* We found a match! */
		data->func = match;

		free(resolved->candidate);
		free(resolved);
		goto end;
	}

	fprintf(stderr,
		"\e[1;91moverload mismatch\e[0m, found \e[92m%d\e[0m potential "
		"candidate(s), but none of them match:\n",
		resolved->n_candidates);
	for (int i = 0; i < resolved->n_candidates; i++) {
		char *sig = fn_str_signature(resolved->candidate[i], true);
		fprintf(stderr, "  %s\n", sig);
		free(sig);
	}

	int max_params, min_params;

	max_params = 0;
	min_params = 0xffff;

	/* Find if something is supposed to take a reference. */
	for (int i = 0; i < resolved->n_candidates; i++) {
		fn_expr_t *match = resolved->candidate[i];

		if (match->n_params > max_params)
			max_params = match->n_params;
		if (match->n_params < min_params)
			min_params = match->n_params;

		if (match->n_params < data->n_args)
			continue;

		for (int j = 0; j < data->n_args; j++) {
			if (match->params[j]->type->type != TY_POINTER)
				continue;
			if (data->args[j]->type != VE_REF)
				continue;

			if (!type_cmp(match->params[j]->type->v_base,
				      data->args[j]->return_type)) {
				continue;
			}

			char *fix;
			fix = calloc(64, 1);
			snprintf(fix, 64, "&%.*s", arg_tokens.tokens[j]->len,
				 arg_tokens.tokens[j]->value);

			error_at_with_fix(
			    tokens->source, arg_tokens.tokens[j]->value,
			    arg_tokens.tokens[j]->len, fix,
			    "%s takes a `%s` here, did you mean to "
			    "pass a reference?",
			    data->name, type_name(match->params[j]->type),
			    data->args[j]->name);
		}
	}

	/* If there is only one candidate, tell the user about matching types */
	if (resolved->n_candidates == 1) {
		fn_expr_t *match = resolved->candidate[0];
		int to_match = data->n_args < match->n_params ? data->n_args
							      : match->n_params;

		for (int i = 0; i < to_match; i++) {
			if (type_cmp(match->params[i]->type,
				     data->args[i]->return_type)) {
				continue;
			}

			error_at(tokens->source, arg_tokens.tokens[i]->value,
				 arg_tokens.tokens[i]->len,
				 "mismatched type, expected `%s` but got `%s`",
				 type_name(match->params[i]->type),
				 type_name(data->args[i]->return_type));
		}
	}

	char more_info[128] = "";

	if (max_params < data->n_args) {
		snprintf(more_info, 128, ", `%s` takes at most %d parameters",
			 data->name, max_params);
	} else if (min_params > data->n_args) {
		snprintf(more_info, 128, ", `%s` takes at least %d parameters",
			 data->name, min_params);
	}

	error_at(tokens->source, fn_name_tok->value, fn_name_tok->len,
		 "could not find a matching overload%s", more_info);

end:
	free(arg_tokens.tokens);
	return ERR_OK;
}

/**
 * ident([arg, ...])
 */
static void parse_call(expr_t *parent, expr_t *mod, token_list *tokens,
		       token *tok)
{
	expr_t *node;
	call_expr_t *data;

	data = calloc(1, sizeof(*data));

	node = expr_add_child(parent);
	node->type = E_CALL;
	node->data = data;
	node->data_free = (expr_free_handle) call_expr_free;

	if (parse_inline_call(parent, mod, data, tokens, tok)
	    == ERR_WAS_BUILTIN) {
		free(data);
		memset(node, 0, sizeof(*node));
		node->type = E_SKIP;
	}
}

static err_t parse_single_value(expr_t *context, expr_t *mod,
				value_expr_t *node, token_list *tokens,
				token *tok)
{
	type_t *temp_type;

	/* literal */
	if (is_literal(tok)) {
		parse_literal(node, tokens, tok);
		return ERR_OK;
	}

	/* call */
	if (is_call(tokens, tok)) {
		node->type = VE_CALL;
		node->call = calloc(1, sizeof(*node->call));
		parse_inline_call(context, mod, node->call, tokens, tok);
		node->return_type = type_copy(node->call->func->return_type);
		return ERR_OK;
	}

	/* reference */
	if (is_reference(tok)) {
		parse_reference(node, context, tokens, tok);
		return ERR_OK;
	}

	/* dereference */
	if (is_dereference(tokens, tok)) {
		tok = next_tok(tokens);
		parse_reference(node, context, tokens, tok);
		node->type = VE_DEREF;
		temp_type = node->return_type;
		node->return_type = type_copy(node->return_type->v_base);
		type_destroy(temp_type);
		return ERR_OK;
	}

	/* pointer-to */
	if (is_pointer_to(tokens, tok)) {
		tok = next_tok(tokens);
		parse_reference(node, context, tokens, tok);
		node->type = VE_PTR;
		temp_type = node->return_type;
		node->return_type = type_pointer_of(node->return_type);
		type_destroy(temp_type);
		return ERR_OK;
	}

	return ERR_SYNTAX;
}

static int twoside_find_op(token_list *tokens, token *tok)
{
	int offset = 0;

	while (tok->type != T_OPERATOR)
		tok = index_tok(tokens, tokens->iter + offset++);
	return tokens->iter + (offset - 1);
}

static value_expr_t *parse_twoside_value_expr(expr_t *context, expr_t *mod,
					      value_expr_t *node,
					      token_list *tokens, token *tok)
{
	token *left, *op, *right, *after_right;

	left = tok;
	op = index_tok(tokens, twoside_find_op(tokens, tok));
	right = index_tok(tokens, twoside_find_op(tokens, tok) + 1);
	after_right = index_tok(tokens, tokens->iter + 2);

	if (op->type != T_OPERATOR) {
		error_at(tokens->source, op->value, op->len,
			 "expected operator, got `%.*s`", op->len, op->value);
	}

	if (!is_single_value(tokens, right)) {
		error_at(tokens->source, right->value, op->len,
			 "expected some value, got `%.*s`", right->len,
			 right->value);
	}

	/* we have a two-sided value with an operator */
	node->type = value_expr_type_from_op(op);

	/* if there is a left node defined, it means that is already has been
	   parsed and we only want to parse the right hand side */
	if (!node->left) {
		node->left = calloc(1, sizeof(*node->left));
		if (parse_single_value(context, mod, node->left, tokens,
				       left)) {
			error_at(tokens->source, left->value, op->len,
				 "syntax error when parsing value");
		}
		next_tok(tokens);
	}

	/* right hand side */
	node->right = calloc(1, sizeof(*node->right));
	parse_single_value(context, mod, node->right, tokens, right);
	next_tok(tokens);

	/* resolve the return type of the expression; for now, just
	   assume that it's the same of whateever the return type of the
	   left operand is */
	node->return_type = type_copy(node->left->return_type);

	/* if the is an operator after this expression, set this whole
	   expression to the left-hand side, and pass it again to a
	   parse_twoside */
	if (after_right->type == T_OPERATOR) {
		value_expr_t *new_node;
		new_node = calloc(1, sizeof(*new_node));
		new_node->left = node;
		node = parse_twoside_value_expr(context, mod, new_node, tokens,
						right);
		next_tok(tokens);
	}

	return node;
}

/**
 * literal   ::= string | integer | float | "null"
 * reference ::= identifier
 * value     ::= literal
 *           ::= reference
 *           ::= call
 *           ::= dereference
 *           ::= pointer-to
 *           ::= value op value
 *           ::= op value
 *
 * value
 */
static value_expr_t *parse_value_expr(expr_t *context, expr_t *mod,
				      value_expr_t *node, token_list *tokens,
				      token *tok)
{
	/* literal | reference | call | dereference | pointer-to */
	if (((is_literal(tok) || is_reference(tok) || is_pointer_to(tokens, tok)
	      || is_dereference(tokens, tok))
	     && index_tok(tokens, tokens->iter)->type == T_NEWLINE)
	    || is_call(tokens, tok)) {
		parse_single_value(context, mod, node, tokens, tok);
		return node;
	}

	/* value op value */
	if (is_single_value(tokens, tok)) {
		/* TODO: this operator parser can only do single chain of
		   values, without any parenthesis or operator precendence */
		return parse_twoside_value_expr(context, mod, node, tokens,
						tok);
	}

	error_at(tokens->source, tok->value, tok->len, "failed to parse value");
	return node;
}

/**
 * name[: type] = value
 */
static bool is_var_assign(token_list *tokens, token *tok)
{
	int offset = 0;

	if (TOK_IS(tok, T_OPERATOR, "*")) {
		tok = index_tok(tokens, tokens->iter);
		offset++;
	}

	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter + offset);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		offset++;
		tok = index_tok(tokens, tokens->iter + offset);
		if (tok->type != T_DATATYPE)
			return false;
		offset++;
	}

	tok = index_tok(tokens, tokens->iter + offset);
	if (!TOK_IS(tok, T_OPERATOR, "="))
		return false;

	return true;
}

static void fn_add_local_var(fn_expr_t *func, var_decl_expr_t *var)
{
	func->locals = realloc(func->locals, sizeof(var_decl_expr_t *)
						 * (func->n_locals + 1));
	func->locals[func->n_locals++] = var;
}

/**
 * name: type
 */
static err_t parse_var_decl(expr_t *parent, fn_expr_t *fn, token_list *tokens,
			    token *tok)
{
	var_decl_expr_t *data;
	expr_t *node;

	if (node_has_named_local(parent, tok->value, tok->len)) {
		error_at(tokens->source, tok->value, tok->len,
			 "a variable named `%.*s` already has been "
			 "declared in "
			 "this scope",
			 tok->len, tok->value);
	}

	data = calloc(sizeof(*data), 1);
	data->name = strndup(tok->value, tok->len);
	data->decl_location = tok;

	tok = next_tok(tokens);
	tok = next_tok(tokens);
	data->type = type_from_sized_string(tok->value, tok->len);

	node = expr_add_child(parent);
	node->type = E_VARDECL;
	node->data = data;
	node->data_free = (expr_free_handle) var_decl_expr_free;

	fn_add_local_var(fn, data);

	return ERR_OK;
}

/**
 * name[: type] = value
 */
static err_t parse_assign(expr_t *parent, expr_t *mod, fn_expr_t *fn,
			  token_list *tokens, token *tok)
{
	assign_expr_t *data;
	expr_t *node;
	token *name;
	bool deref = false;
	bool guess_type = false;
	expr_t *guess_decl;

	name = tok;

	if (TOK_IS(tok, T_OPERATOR, "*")) {
		deref = true;
		tok = next_tok(tokens);
		name = tok;
	}

	/* defined variable type, meaning we declare a new variable */
	if (is_var_decl(tokens, tok)) {
		parse_var_decl(parent, fn, tokens, tok);
	} else if (!node_has_named_local(parent, name->value, name->len)) {
		if (deref) {
			error_at(tokens->source, name->value, name->len,
				 "use of an undeclared variable");
		}
		guess_type = true;
	}

	if (guess_type) {
		guess_decl = expr_add_child(parent);
		guess_decl->type = E_VARDECL;
		guess_decl->data = calloc(1, sizeof(var_decl_expr_t));
		guess_decl->data_free = (expr_free_handle) var_decl_expr_free;
		E_AS_VDECL(guess_decl->data)->name =
		    strndup(name->value, name->len);
	}

	node = expr_add_child(parent);
	data = calloc(1, sizeof(*data));
	data->to = calloc(1, sizeof(*data->to));
	data->to->type = deref ? VE_DEREF : VE_REF;
	data->to->name = strndup(name->value, name->len);

	node->type = E_ASSIGN;
	node->data = data;
	node->data_free = (expr_free_handle) assign_expr_free;

	/* parse value */
	tok = next_tok(tokens);
	tok = next_tok(tokens);

	data->value = calloc(1, sizeof(*data->value));
	data->value = parse_value_expr(parent, mod, data->value, tokens, tok);

	if (guess_type) {
		data->to->return_type = type_copy(data->value->return_type);

		/* If we guessed the var type, declare a new variable. */
		E_AS_VDECL(guess_decl->data)->type =
		    type_copy(data->to->return_type);
		fn_add_local_var(fn, guess_decl->data);
	} else {
		/* We are assigning to a regular var. Note that we are not
		   touching the resolved local, so that we can later check if it
		   has been used. */
		var_decl_expr_t *local =
		    node_resolve_local_touch(parent, data->to->name, 0, false);

		if (!local)
			error("parser: failed to find local var decl of deref");

		if (deref) {
			if (local->type->type != TY_POINTER) {
				error_at(
				    tokens->source, name->value, name->len,
				    "cannot dereference a non-pointer type");
			}

			data->to->return_type = type_copy(local->type->v_base);
		} else {
			data->to->return_type = type_copy(local->type);
		}
	}

	if (!type_cmp(data->to->return_type, data->value->return_type)) {
		error_at(tokens->source, name->value, name->len,
			 "mismatched types in assignment: left is `%s` and "
			 "right is `%s`",
			 type_name(data->to->return_type),
			 type_name(data->value->return_type));
	}

	return ERR_OK;
}

/**
 * ret [value]
 */
static err_t parse_return(expr_t *parent, expr_t *mod, token_list *tokens,
			  token *tok)
{
	type_t *return_type;
	value_expr_t *data;
	expr_t *node;

	node = expr_add_child(parent);
	data = calloc(1, sizeof(*data));

	node->type = E_RETURN;
	node->data = data;
	node->data_free = (expr_free_handle) value_expr_free;

	return_type = type_new_null();
	if (parent->type == E_FUNCTION) {
		type_destroy(return_type);
		return_type = type_copy(E_AS_FN(parent->data)->return_type);
	}

	tok = next_tok(tokens);
	if (tok->type == T_NEWLINE && return_type->type != TY_NULL) {
		error_at(tokens->source, tok->value, tok->len,
			 "missing return value for function that "
			 "returns `%s`",
			 type_name(return_type));
	}

	if (tok->type != T_NEWLINE && return_type->type == TY_NULL) {
		error_at(tokens->source, tok->value, tok->len,
			 "cannot return value, because `%s` returns null",
			 E_AS_FN(parent->data)->name);
	}

	/* return without a value */
	if (return_type->type == TY_NULL) {
		data->return_type = type_new_null();
		data->type = VE_NULL;
		return ERR_OK;
	}

	/* return with a value */
	node->data = parse_value_expr(parent, mod, data, tokens, tok);
	data = node->data;

	if (!type_cmp(data->return_type, E_AS_FN(parent->data)->return_type)) {
		error_at(tokens->source, tok->value, tok->len,
			 "mismatched return type: expression returns "
			 "%s, while "
			 "the function returns %s",
			 type_name(data->return_type),
			 type_name(E_AS_FN(parent->data)->return_type));
	}

	type_destroy(return_type);

	return ERR_OK;
}

static bool fn_has_return(expr_t *func)
{
	expr_t *walker = func->child;
	if (!walker)
		return false;

	do {
		if (walker->type == E_RETURN)
			return true;
	} while ((walker = walker->next));

	return false;
}

/**
 * fn name([arg: type], ...)[-> return_type]
 */
static fn_expr_t *parse_fn_decl(expr_t *module, fn_expr_t *decl,
				token_list *tokens, token *tok)
{
	token *return_type_tok;
	token *name;

	decl->n_params = 0;
	decl->return_type = type_new_null();

	tok = next_tok(tokens);

	/* name */
	if (tok->type != T_IDENT) {
		error_at(tokens->source, tok->value, tok->len,
			 "expected function name, got %s", tokname(tok->type));
	}

	name = tok;
	decl->name = strndup(tok->value, tok->len);

	/* parameters (optional) */
	tok = next_tok(tokens);
	if (!TOK_IS(tok, T_PUNCT, "(")) {
		goto params_skip;
	}

	tok = next_tok(tokens);
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		type_t *type;
		token *name;

		if (tok->type == T_DATATYPE || TOK_IS(tok, T_PUNCT, "&")) {
			char fix[128];
			int errlen;
			type = parse_type(tokens, tok);
			name = index_tok(tokens, tokens->iter);
			errlen = tok->len;

			snprintf(fix, 128, "%.*s: %s", name->len, name->value,
				 type_name(type));

			if (name->type != T_IDENT) {
				snprintf(fix, 128, "%s: %s",
					 type_example_varname(type),
					 type_name(type));
			}

			if (TOK_IS(tok, T_PUNCT, "&"))
				errlen +=
				    index_tok(tokens, tokens->iter - 1)->len;

			error_at_with_fix(
			    tokens->source, tok->value, errlen, fix,
			    "the parameter name comes first, not the type");
		}

		if (tok->type != T_IDENT) {
			char fix[128];
			name = index_tok(tokens, tokens->iter + 1);
			type = parse_type(tokens, tok);

			snprintf(fix, 128, "%.*s: %s", name->len, name->value,
				 type_name(type));

			if (TOK_IS(tok, T_PUNCT, "&")) {
				error_at(tokens->source, tok->value, tok->len,
					 "the address marker should be next to "
					 "the type, not the variable name");
			}

			error_at(tokens->source, tok->value, tok->len,
				 "missing parameter name");
		}

		name = tok;
		tok = next_tok(tokens);

		if (!TOK_IS(tok, T_PUNCT, ":")) {
			char fix[64];
			snprintf(fix, 64, "%.*s: T", name->len, name->value);
			error_at_with_fix(
			    tokens->source, name->value, name->len, fix,
			    "the `%.*s` parameter is missing a type", name->len,
			    name->value);
		}

		tok = next_tok(tokens);

		/* get the data type */

		type = parse_type(tokens, tok);
		tok = next_tok(tokens);

		if (!TOK_IS(tok, T_PUNCT, ",") && !TOK_IS(tok, T_PUNCT, ")")) {
			error_at_with_fix(tokens->source, tok->value, tok->len,
					  ", or )", "unexpected token");
		}

		fn_add_param(decl, name->value, name->len, type);
		type_destroy(type);

		if (TOK_IS(tok, T_PUNCT, ","))
			tok = next_tok(tokens);
	}

	/* return type (optional) */
	tok = next_tok(tokens);
	return_type_tok = NULL;

params_skip:
	if (TOK_IS(tok, T_OPERATOR, "->")) {
		tok = next_tok(tokens);

		if (tok->type != T_DATATYPE) {
			error_at(tokens->source, tok->value, tok->len,
				 "expected return type, got `%.*s`", tok->len,
				 tok->value);
		}

		if (decl->return_type)
			type_destroy(decl->return_type);
		decl->return_type =
		    type_from_sized_string(tok->value, tok->len);
		return_type_tok = tok;
		tok = next_tok(tokens);
	}

	if (!strcmp(decl->name, "main")) {
		if (decl->n_params) {
			error_at(tokens->source, name->value, name->len,
				 "the main function does not take any "
				 "arguments");
		}

		if (decl->return_type->type != TY_NULL) {
			error_at(tokens->source, return_type_tok->value,
				 return_type_tok->len,
				 "the main function cannot return `%.*s`, as "
				 "it always returns `null`",
				 return_type_tok->len, return_type_tok->value);
		}
	}

	return NULL;
}

static void fn_warn_unused(file_t *source, expr_t *fn)
{
	var_decl_expr_t *decl;
	expr_t *walker;

	walker = fn->child;
	do {
		if (walker->type != E_VARDECL)
			continue;

		decl = walker->data;
		if (decl->used)
			continue;

		/* We found an unused variable. */
		if (!decl->decl_location) {
			warning("unused variable %s", decl->name);
		} else {
			warning_at(source, decl->decl_location->value,
				   decl->decl_location->len, "unused variable");
		}

	} while ((walker = walker->next));
}

/**
 * {
 *   [expression]...
 * }
 */
static err_t parse_fn_body(expr_t *module, fn_expr_t *decl, token_list *tokens)
{
	expr_t *node;
	token *tok;

	node = expr_add_child(module);

	node->type = E_FUNCTION;
	node->data = decl;
	node->data_free = (expr_free_handle) fn_expr_free;

	tok = next_tok(tokens);

	/* opening & closing braces */
	while (tok->type == T_NEWLINE)
		tok = next_tok(tokens);

	if (!TOK_IS(tok, T_PUNCT, "{")) {
		error_at(tokens->source, tok->value, tok->len,
			 "missing opening brace for `%s`", decl->name);
		return ERR_SYNTAX;
	}

	int brace_level = 1;
	while (brace_level != 0) {
		tok = next_tok(tokens);

		if (tok->type == T_END) {
			if (brace_level == 1)
				error_at(tokens->source, tok->value, tok->len,
					 "missing a closing brace");
			else
				error_at(tokens->source, tok->value, tok->len,
					 "missing %d closing braces",
					 brace_level);
		}

		if (tok->type == T_NEWLINE)
			continue;

		if (TOK_IS(tok, T_PUNCT, "{")) {
			brace_level++;
			continue;
		}

		if (TOK_IS(tok, T_PUNCT, "}")) {
			brace_level--;
			continue;
		}

		/* statements inside the function */

		if (is_var_assign(tokens, tok))
			parse_assign(node, module, decl, tokens, tok);
		else if (is_var_decl(tokens, tok))
			parse_var_decl(node, decl, tokens, tok);
		else if (is_call(tokens, tok))
			parse_call(node, module, tokens, tok);
		else if (TOK_IS(tok, T_KEYWORD, "ret"))
			parse_return(node, module, tokens, tok);
		else if (TOK_IS(tok, T_IDENT, "return"))
			error_at_with_fix(tokens->source, tok->value, tok->len,
					  "ret", "did you mean `ret`?");
		else
			error_at(tokens->source, tok->value, tok->len,
				 "unparsable");
	}

	/* always add a return statement */
	if (!fn_has_return(node)) {
		if (decl->return_type->type != TY_NULL) {
			error_at(tokens->source, tok->value, tok->len,
				 "missing return statement for %s", decl->name);
		}

		expr_t *ret_expr = expr_add_child(node);
		value_expr_t *val = calloc(1, sizeof(*val));

		ret_expr->type = E_RETURN;
		ret_expr->data_free = (expr_free_handle) value_expr_free;
		ret_expr->data = val;
		val->type = VE_NULL;
	}

	fn_warn_unused(tokens->source, node);

	return 0;
}

typedef struct
{
	int tok_index;
	fn_expr_t *decl;
} fn_pos_t;

static fn_pos_t *acquire_fn_pos(fn_pos_t **pos, int *n)
{
	*pos = realloc(*pos, sizeof(fn_pos_t) * (*n + 1));
	return &(*pos)[(*n)++];
}

static void skip_block(token_list *tokens, token *tok)
{
	int depth = 0;

	do {
		if (TOK_IS(tok, T_PUNCT, "{"))
			depth++;
		if (TOK_IS(tok, T_PUNCT, "}"))
			depth--;
		tok = next_tok(tokens);
	} while (depth);
}

static void parse_use(settings_t *settings, expr_t *module, token_list *tokens,
		      token *tok)
{
	token *tmp_tok;
	token *start, *end;
	char *path;

	tok = next_tok(tokens);
	start = tok;

	int offset = 0;
	while ((end = index_tok(tokens, tokens->iter + offset))->type
	       != T_NEWLINE) {
		offset++;
	}

	int n = end->value - start->value;

	if (tok->type == T_STRING) {
		path = strndup(tok->value, tok->len);
		if (!module_import(settings, module, path)) {
			error_at(tokens->source, start->value - 1, n + 1,
				 "cannot find module");
		}

		free(path);
		return;
	}

	if (tok->type != T_IDENT) {
		error_at(tokens->source, tok->value, tok->len,
			 "expected module name or path");
	}

	/* Collect path */
	path = calloc(512, 1);
	do {
		if (tok->type != T_IDENT) {
			error_at(tokens->source, tok->value, tok->len,
				 "expected module name");
		}

		strcat(path, "/");
		strncat(path, tok->value, tok->len);

		tok = next_tok(tokens);

		if (tok->type == T_NEWLINE)
			break;

		if (!TOK_IS(tok, T_PUNCT, ".")) {
			error_at_with_fix(
			    tokens->source, tok->value, tok->len, ".",
			    "expected dot seperator after module name");
		}

		/* Warn the user about dots at the end. */
		tmp_tok = index_tok(tokens, tokens->iter);
		if (tmp_tok->type == T_NEWLINE) {
			warning_at(tokens->source, tmp_tok->value - 1,
				   tmp_tok->len,
				   "unnecessary dot, you can remove it");
		}

	} while ((tok = next_tok(tokens))->type != T_NEWLINE);

	if (!module_std_import(settings, module, path)) {
		error_at(tokens->source, start->value, n,
			 "cannot find module in standard library");
	}

	free(path);
}

expr_t *parse(settings_t *settings, token_list *tokens, const char *module_id)
{
	token *current = next_tok(tokens);
	expr_t *module = calloc(sizeof(*module), 1);
	mod_expr_t *data;

	fn_pos_t *fn_pos;
	int n_fn_pos;

	data = calloc(1, sizeof(mod_expr_t));
	data->name = strdup(module_id);
	data->source_name = strdup(tokens->source->path);

	module->type = E_MODULE;
	module->data = data;
	module->data_free = (expr_free_handle) mod_expr_free;

	/*
	 * In order to support overloading & use-before-declare we need
	 * to parse the declarations before the contents.
	 */

	fn_pos = NULL;
	n_fn_pos = 0;

	while ((current = next_tok(tokens)) && current->type != T_END) {
		/* top-level: function decl */
		if (TOK_IS(current, T_KEYWORD, "fn")) {

			/* parse only the fn declaration, leave the rest */
			fn_pos_t *pos = acquire_fn_pos(&fn_pos, &n_fn_pos);
			pos->decl = module_add_local_decl(module);
			parse_fn_decl(module, pos->decl, tokens, current);
			pos->tok_index = tokens->iter - 1;
			current = index_tok(tokens, tokens->iter - 1);

			/* skip the function body for now */
			skip_block(tokens, current);

		} else if (current->type == T_NEWLINE) {
			continue;
		} else if (TOK_IS(current, T_KEYWORD, "use")) {
			parse_use(settings, module, tokens, current);
		} else if (is_builtin_function(current)) {
			parse_builtin_call(module, module, tokens, current);
		} else {
			error_at(
			    tokens->source, current->value, current->len,
			    "only functions and imports are allowed at the "
			    "top-level");
			goto err;
		}
	}

	/* Go back and parse the function contents */
	for (int i = 0; i < n_fn_pos; i++) {
		tokens->iter = fn_pos[i].tok_index;
		parse_fn_body(module, fn_pos[i].decl, tokens);
	}

	free(fn_pos);

err:
	return module;
}

void expr_destroy(expr_t *expr)
{
	if (!expr)
		return;

	if (expr->child)
		expr_destroy(expr->child);
	if (expr->next)
		expr_destroy(expr->next);

	if (expr->data) {
		if (expr->data_free)
			expr->data_free(expr->data);
		free(expr->data);
	}

	free(expr);
}

const char *expr_typename(expr_type type)
{
	static const char *names[] = {"SKIP",   "MODULE",  "FUNCTION", "CALL",
				      "RETURN", "VARDECL", "ASSIGN",   "VALUE"};

	if (type >= 0 && type < LEN(names))
		return names[type];
	return "<EXPR>";
}

char *fn_str_signature(fn_expr_t *func, bool with_colors)
{
	char *sig = calloc(1024, 1);
	char buf[64];
	char *ty_str;
	memset(sig, 0, 1024);

	snprintf(sig, 1024, "%s%s%s(", with_colors ? "\e[94m" : "", func->name,
		 with_colors ? "\e[0m" : "");

	for (int i = 0; i < func->n_params - 1; i++) {
		ty_str = type_name(func->params[i]->type);
		snprintf(buf, 64, "%s: %s, ", func->params[i]->name, ty_str);
		strcat(sig, buf);
		free(ty_str);
	}

	if (func->n_params > 0) {
		ty_str = type_name(func->params[func->n_params - 1]->type);
		snprintf(buf, 64, "%s: %s",
			 func->params[func->n_params - 1]->name, ty_str);
		strcat(sig, buf);
		free(ty_str);
	}

	ty_str = type_name(func->return_type);
	snprintf(buf, 64, "): %s", ty_str);
	strcat(sig, buf);
	free(ty_str);

	return sig;
}

static const char *expr_info(expr_t *expr)
{
	static char info[512];
	assign_expr_t *var;
	char *tmp = NULL;
	char marker;

	switch (expr->type) {
	case E_MODULE:
		snprintf(info, 512, "%s", E_AS_MOD(expr->data)->name);
		break;
	case E_FUNCTION:
		tmp = fn_str_signature(E_AS_FN(expr->data), true);
		snprintf(info, 512, "%s", tmp);
		break;
	case E_VARDECL:
		tmp = type_name(E_AS_VDECL(expr->data)->type);
		snprintf(info, 512, "%s %s", tmp, E_AS_VDECL(expr->data)->name);
		break;
	case E_ASSIGN:
		var = E_AS_ASS(expr->data);
		tmp = type_name(var->value->return_type);

		if (var->to->type == VE_LIT || var->to->type == VE_REF)
			marker = ' ';
		else if (var->to->type == VE_PTR)
			marker = '&';
		else if (var->to->type == VE_DEREF)
			marker = '*';
		else
			marker = '?';

		snprintf(info, 512, "%c%s = (%s) %s", marker, var->to->name,
			 tmp, value_expr_type_name(var->value->type));
		break;
	case E_RETURN:
		tmp = type_name(E_AS_VAL(expr->data)->return_type);
		snprintf(info, 512, "%s %s", tmp,
			 value_expr_type_name(E_AS_VAL(expr->data)->type));
		break;
	case E_CALL:
		snprintf(info, 512, "`%s` n_args=%d",
			 E_AS_CALL(expr->data)->name,
			 E_AS_CALL(expr->data)->n_args);
		break;
	default:
		info[0] = 0;
	}

	free(tmp);
	return info;
}

static void expr_print_level(expr_t *expr, int level, bool with_next);

static void expr_print_value_expr(value_expr_t *val, int level)
{
	char *lit_str;
	char *tmp = NULL;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	if (val->type == VE_NULL) {
		printf("literal: null\n");
	} else if (val->type == VE_REF) {
		printf("ref: `%s`\n", val->name);
	} else if (val->type == VE_LIT) {
		lit_str = stringify_literal(val->literal);
		tmp = type_name(val->literal->type);
		printf("literal: %s %s\n", tmp, lit_str);
		free(lit_str);
	} else if (val->type == VE_CALL) {
		printf("call: `%s` n_args=%d\n", val->call->name,
		       val->call->n_args);
		for (int i = 0; i < val->call->n_args; i++)
			expr_print_value_expr(val->call->args[i], level + 1);
	} else if (val->type == VE_PTR) {
		printf("addr: `&%s`\n", val->name);
	} else if (val->type == VE_DEREF) {
		printf("deref: `*%s`\n", val->name);
	} else {
		printf("op: %s\n", value_expr_type_name(val->type));
		if (val->left)
			expr_print_value_expr(val->left, level + 1);
		if (val->right)
			expr_print_value_expr(val->right, level + 1);
	}

	free(tmp);
}

static void expr_print_level(expr_t *expr, int level, bool with_next)
{
	expr_t *walker;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	printf("\e[96m%s\e[0m %s\n", expr_typename(expr->type),
	       expr_info(expr));

	if (expr->type == E_RETURN)
		expr_print_value_expr(expr->data, level + 1);

	if (expr->type == E_ASSIGN)
		expr_print_value_expr(E_AS_ASS(expr->data)->value, level + 1);

	if (expr->type == E_CALL) {
		for (int i = 0; i < E_AS_CALL(expr->data)->n_args; i++) {
			expr_print_value_expr(E_AS_CALL(expr->data)->args[i],
					      level + 1);
		}
	}

	if (expr->child)
		expr_print_level(expr->child, level + 1, true);

	if (with_next) {
		walker = expr;
		while (walker && (walker = walker->next))
			expr_print_level(walker, level, false);
	}
}

void expr_print(expr_t *expr)
{
	expr_print_level(expr, 0, true);
}

void literal_default(literal_expr_t *literal)
{
	type_t *t = literal->type;

	switch (literal->type->v_plain) {
	case PT_STR:
		literal->v_str.len = 0;
		literal->v_str.ptr = "";
		break;
	default:
		memset(literal, 0, sizeof(*literal));
		literal->type = t;
	}
}

char *stringify_literal(literal_expr_t *literal)
{
	if (literal->type->type != TY_PLAIN)
		error("literal cannot be of non-plain type");

	if (literal->type->v_plain == PT_I32) {
		char *buf = malloc(16);
		snprintf(buf, 16, "%d", literal->v_i32);
		return buf;
	}

	if (literal->type->v_plain == PT_STR)
		return strndup(literal->v_str.ptr, literal->v_str.len);

	if (literal->type == TY_NULL) {
		char *buf = malloc(5);
		strcpy(buf, "null");
		return buf;
	}

	return NULL;
}

const char *value_expr_type_name(value_expr_type t)
{
	static const char *names[] = {"NULL", "REF", "LIT", "CALL", "ADD",
				      "SUB",  "MUL", "DIV", "PTR",  "DEREF"};

	if (t >= 0 && t < LEN(names))
		return names[t];
	return "<value expr type>";
}

value_expr_type value_expr_type_from_op(token *op)
{
	if (op->type != T_OPERATOR)
		return VE_NULL;

	static const struct
	{
		const char *val;
		value_expr_type type;
	} ops[] = {{"+", VE_ADD}, {"-", VE_SUB}, {"*", VE_MUL}, {"/", VE_DIV}};

	for (int i = 0; i < LEN(ops); i++) {
		if (!strncmp(op->value, ops[i].val, op->len))
			return ops[i].type;
	}

	return VE_NULL;
}
