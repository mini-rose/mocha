#include <ctype.h>
#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
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
	free(module->name);
	free(module->source_name);
	for (int i = 0; i < module->n_decls; i++) {
		fn_expr_free(module->decls[i]);
		free(module->decls[i]);
	}
	free(module->decls);
}

static void fn_expr_free(fn_expr_t *function)
{
	int i;

	free(function->name);

	for (i = 0; i < function->n_params; i++) {
		free(function->params[i]->name);
		free(function->params[i]);
	}

	free(function->locals);
	free(function->params);
}

static void value_expr_free(value_expr_t *value)
{
	if (!value)
		return;

	if (value->type == VE_NULL)
		return;
	else if (value->type == VE_REF) {
		free(value->name);
	} else if (value->type == VE_LIT) {
		if (value->literal->type == PT_STR)
			free(value->literal->v_str.ptr);
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
	free(assign->name);
	free(assign->value);
}

static void var_decl_expr_free(var_decl_expr_t *variable)
{
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

void fn_add_param(fn_expr_t *fn, const char *name, int len, plain_type type)
{
	fn->params =
	    realloc(fn->params, sizeof(var_decl_expr_t) * ++fn->n_params);
	fn->params[fn->n_params - 1] = malloc(sizeof(var_decl_expr_t));
	fn->params[fn->n_params - 1]->name = strndup(name, len);
	fn->params[fn->n_params - 1]->type = type;
}

var_decl_expr_t *node_resolve_local(expr_t *node, const char *name, int len)
{
	if (len == 0)
		len = strlen(name);

	/* only support functions now */
	if (node->type != E_FUNCTION)
		return NULL;

	fn_expr_t *fn = node->data;

	for (int i = 0; i < fn->n_params; i++) {
		if (!strncmp(fn->params[i]->name, name, len))
			return fn->params[i];
	}

	for (int i = 0; i < fn->n_locals; i++) {
		if (!strncmp(fn->locals[i]->name, name, len))
			return fn->locals[i];
	}

	return NULL;
}

bool node_has_named_local(expr_t *node, const char *name, int len)
{
	return node_resolve_local(node, name, len) != NULL;
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

/*
 * value ::= literal
 *       ::= ident
 *       ::= call
 */
static bool is_single_value(token_list *tokens, token *tok)
{
	return is_literal(tok) || is_reference(tok) || is_call(tokens, tok);
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

static void parse_literal(value_expr_t *node, token_list *tokens, token *tok)
{
	node->type = VE_LIT;

	/* string */
	if (tok->type == T_STRING) {
		node->literal = calloc(1, sizeof(*node->literal));
		node->return_type = PT_STR;
		node->literal->type = PT_STR;
		node->literal->v_str.ptr = strndup(tok->value, tok->len);
		node->literal->v_str.len = tok->len;
	}

	if (tok->type == T_NUMBER) {
		node->literal = calloc(1, sizeof(*node->literal));
		char *tmp = strndup(tok->value, tok->len);

		if (is_integer(tok)) {
			node->return_type = PT_I32;
			node->literal->type = PT_I32;
			node->literal->v_i32 = strtol(tmp, NULL, 10);
		} else if (is_float(tok)) {
			node->return_type = PT_F32;
			node->literal->type = PT_F32;
			node->literal->v_f32 = strtof(tmp, NULL);
		}

		free(tmp);
	}

	/* "null" */
	if (TOK_IS(tok, T_DATATYPE, "null")) {
		node->return_type = PT_NULL;
		node->type = VE_NULL;
	}
}

static void parse_reference(value_expr_t *node, expr_t *context,
			    token_list *tokens, token *tok)
{
	if (!node_has_named_local(context, tok->value, tok->len)) {
		error_at(tokens->source->content, tok->value,
			 "use of undeclared variable: `%.*s`", tok->len,
			 tok->value);
	}

	node->type = VE_REF;
	node->name = strndup(tok->value, tok->len);

	var_decl_expr_t *var =
	    node_resolve_local(context, tok->value, tok->len);
	node->return_type = var->type;
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
	static const int n_builtins = sizeof(builtins) / sizeof(*builtins);

	for (int i = 0; i < n_builtins; i++) {
		if (!strncmp(builtins[i], name->value, name->len))
			return true;
	}

	return false;
}

static void collect_builtin_decl_arguments(fn_expr_t *decl, token_list *tokens,
					   int arg_pos)
{
	token *arg;

	while ((arg = index_tok(tokens, arg_pos++))->type != T_NEWLINE) {
		if (arg->type != T_DATATYPE) {
			error_at(tokens->source->content, arg->value,
				 "expected function parameter type, got `%.*s`",
				 arg->len, arg->value);
		}

		fn_add_param(decl, "_", 1,
			     plain_type_from(arg->value, arg->len));

		arg = index_tok(tokens, arg_pos++);
		if (TOK_IS(arg, T_PUNCT, ")"))
			break;

		if (!TOK_IS(arg, T_PUNCT, ",")) {
			error_at(tokens->source->content, arg->value,
				 "expected comma between arguments, got `%.*s`",
				 arg->len, arg->value);
		}
	}
}

static err_t parse_builtin_call(expr_t *parent, expr_t *mod, call_expr_t *data,
				token_list *tokens, token *tok)
{
	/* Built-in calls are not always function calls, they may be
	   "macro-like" functions which turn into something else. */

	int arg_pos = tokens->iter + 1;
	token *name, *arg;

	name = tok;
	while (!TOK_IS(tok, T_PUNCT, ")"))
		tok = next_tok(tokens);

	if (!strncmp("__builtin_decl", name->value, name->len)) {
		fn_expr_t *decl = module_add_decl(mod);

		decl->flags = FN_NOMANGLE;

		/* function name */
		arg = index_tok(tokens, arg_pos++);
		if (arg->type != T_STRING) {
			error_at(tokens->source->content, arg->value,
				 "first argument to __builtin_decl must be a "
				 "string with the function name");
		}

		decl->name = strndup(arg->value, arg->len);

		arg = index_tok(tokens, arg_pos++);
		if (!TOK_IS(arg, T_PUNCT, ",")) {
			error_at(tokens->source->content, arg->value,
				 "missing return type argument");
		}

		/* return type */
		arg = index_tok(tokens, arg_pos++);
		if (arg->type != T_DATATYPE) {
			error_at(tokens->source->content, arg->value,
				 "second argument to __builtin_decl is "
				 "expected to be the return type");
		}

		decl->return_type = plain_type_from(arg->value, arg->len);

		arg = index_tok(tokens, arg_pos++);
		if (TOK_IS(arg, T_PUNCT, ")"))
			return ERR_WAS_BUILTIN;

		if (!TOK_IS(arg, T_PUNCT, ",")) {
			error_at(tokens->source->content, arg->value,
				 "expected comma between arguments, got `%.*s`",
				 arg->len, arg->value);
		}

		collect_builtin_decl_arguments(decl, tokens, arg_pos);

		return ERR_WAS_BUILTIN;
	} else {
		error_at(tokens->source->content, tok->value,
			 "this builtin call is not yet implemented");
	}

	return ERR_OK;
}

static err_t parse_inline_call(expr_t *parent, expr_t *mod, call_expr_t *data,
			       token_list *tokens, token *tok)
{
	value_expr_t *arg;
	token *fn_name_tok;

	if (is_builtin_function(tok))
		return parse_builtin_call(parent, mod, data, tokens, tok);

	data->name = strndup(tok->value, tok->len);
	fn_name_tok = tok;

	tok = next_tok(tokens);
	tok = next_tok(tokens);

	/* arguments - currently only support variable names & literals
	 */
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		if (tok->type == T_IDENT) {
			arg = call_add_arg(data);
			arg->type = VE_REF;
			arg->name = strndup(tok->value, tok->len);

			var_decl_expr_t *var;

			var = node_resolve_local(parent, arg->name,
						 strlen(arg->name));
			if (!var) {
				error_at(tokens->source->content, tok->value,
					 "use of undeclared variable: `%.*s`",
					 tok->len, tok->value);
			}

			arg->return_type = var->type;

		} else if (is_literal(tok)) {
			arg = call_add_arg(data);
			parse_literal(arg, tokens, tok);
		} else {
			error_at(tokens->source->content, tok->value,
				 "expected value or variable name, got "
				 "`%.*s`",
				 tok->len, tok->value);
		}

		tok = next_tok(tokens);

		if (TOK_IS(tok, T_PUNCT, ")"))
			break;

		if (!TOK_IS(tok, T_PUNCT, ",")) {
			error_at(tokens->source->content, tok->value,
				 "expected comma seperating the "
				 "arguments, got "
				 "`%.*s`",
				 tok->len, tok->value);
		}

		tok = next_tok(tokens);
	}

	fn_expr_t *target = module_find_fn(mod, data->name);
	data->func = target;

	/* TODO: resolve functions after parsing the whole file, so that
	   functions can be declared anywhere within the file */
	if (!target) {
		error_at(tokens->source->content, fn_name_tok->value,
			 "undeclared function name `%s`", data->name);
	}

	if (target->n_params != data->n_args) {
		error_at(tokens->source->content, fn_name_tok->value,
			 "mismatched argument amount: `%s` takes %d "
			 "arguments",
			 target->name, target->n_params);
	}

	for (int i = 0; i < data->n_args; i++) {
		/* the result of the expression must match the parameter
		 */
		if (data->args[i]->return_type == target->params[i]->type)
			continue;

		error_at(tokens->source->content, fn_name_tok->value,
			 "the type of the %d argument should be `%s`, "
			 "not `%s`",
			 i + 1, plain_type_name(target->params[i]->type),
			 plain_type_name(data->args[i]->return_type));
	}

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
		node->return_type = node->call->func->return_type;
		return ERR_OK;
	}

	/* reference */
	if (is_reference(tok)) {
		parse_reference(node, context, tokens, tok);
		return ERR_OK;
	}

	return ERR_SYNTAX;
}

static value_expr_t *parse_twoside_value_expr(expr_t *context, expr_t *mod,
					      value_expr_t *node,
					      token_list *tokens, token *tok)
{
	token *left, *op, *right, *after_right;

	left = tok;
	op = index_tok(tokens, tokens->iter);
	right = index_tok(tokens, tokens->iter + 1);
	after_right = index_tok(tokens, tokens->iter + 2);

	if (op->type != T_OPERATOR) {
		error_at(tokens->source->content, op->value,
			 "expected operator, got `%.*s`", op->len, op->value);
	}

	if (!is_single_value(tokens, right)) {
		error_at(tokens->source->content, right->value,
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
			error_at(tokens->source->content, left->value,
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
	node->return_type = node->left->return_type;

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
 *           ::= value op value
 *           ::= op value
 *
 * value
 */
static value_expr_t *parse_value_expr(expr_t *context, expr_t *mod,
				      value_expr_t *node, token_list *tokens,
				      token *tok)
{
	/* literal | reference | call */
	if (((is_literal(tok) || is_reference(tok))
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

	/* TODO: op value */
	return node;
}

/**
 * name[: type] = value
 */
static bool is_var_assign(token_list *tokens, token *tok)
{
	int offset = 0;

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
		error_at(tokens->source->content, tok->value,
			 "a variable named `%.*s` already has been "
			 "declared in "
			 "this scope",
			 tok->len, tok->value);
	}

	data = calloc(sizeof(*data), 1);
	data->name = strndup(tok->value, tok->len);

	tok = next_tok(tokens);
	tok = next_tok(tokens);
	data->type = plain_type_from(tok->value, tok->len);

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

	name = tok;

	/* defined variable type, meaning we declare a new variable */
	if (is_var_decl(tokens, tok)) {
		parse_var_decl(parent, fn, tokens, tok);
	} else if (!node_has_named_local(parent, tok->value, tok->len)) {
		error_at(tokens->source->content, tok->value,
			 "use of undeclared variable: `%.*s`", tok->len,
			 tok->value);
	}

	node = expr_add_child(parent);
	data = calloc(1, sizeof(*data));

	data->name = strndup(name->value, name->len);
	node->type = E_ASSIGN;
	node->data = data;
	node->data_free = (expr_free_handle) assign_expr_free;

	/* parse value */
	tok = next_tok(tokens);
	tok = next_tok(tokens);

	data->value = calloc(1, sizeof(*data->value));
	data->value = parse_value_expr(parent, mod, data->value, tokens, tok);

	return ERR_OK;
}

/**
 * ret [value]
 */
static err_t parse_return(expr_t *parent, expr_t *mod, token_list *tokens,
			  token *tok)
{
	plain_type return_type;
	value_expr_t *data;
	expr_t *node;

	node = expr_add_child(parent);
	data = calloc(1, sizeof(*data));

	node->type = E_RETURN;
	node->data = data;
	node->data_free = (expr_free_handle) value_expr_free;

	return_type = PT_NULL;
	if (parent->type == E_FUNCTION)
		return_type = E_AS_FN(parent->data)->return_type;

	tok = next_tok(tokens);
	if (tok->type == T_NEWLINE && return_type != PT_NULL) {
		error_at(tokens->source->content, tok->value,
			 "missing return value for function that "
			 "returns `%s`",
			 plain_type_name(return_type));
	}

	if (tok->type != T_NEWLINE && return_type == PT_NULL) {
		error_at(tokens->source->content, tok->value,
			 "cannot return a value from a function "
			 "returning null");
	}

	/* return without a value */
	if (return_type == PT_NULL) {
		data->return_type = PT_NULL;
		data->type = VE_NULL;
		return ERR_OK;
	}

	/* return with a value */
	node->data = parse_value_expr(parent, mod, data, tokens, tok);
	data = node->data;

	if (data->return_type != E_AS_FN(parent->data)->return_type) {
		error_at(tokens->source->content, tok->value,
			 "mismatched return type: expression returns "
			 "%s, while "
			 "the function returns %s",
			 plain_type_name(data->return_type),
			 plain_type_name(E_AS_FN(parent->data)->return_type));
	}

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

static err_t parse_fn(token_list *tokens, expr_t *module)
{
	fn_expr_t *data;
	expr_t *node;
	token *tok;
	token *name;
	token *return_type_tok;

	node = expr_add_child(module);
	data = calloc(sizeof(*data), 1);

	node->type = E_FUNCTION;
	node->data = data;
	node->data_free = (expr_free_handle) fn_expr_free;
	data->n_params = 0;
	data->return_type = PT_NULL;

	tok = next_tok(tokens);

	/* name */
	if (tok->type != T_IDENT) {
		error_at(tokens->source->content, tok->value,
			 "expected function name, got %s", tokname(tok->type));
		return ERR_SYNTAX;
	}

	name = tok;
	data->name = strndup(tok->value, tok->len);

	/* parameters (optional) */
	tok = next_tok(tokens);
	if (!TOK_IS(tok, T_PUNCT, "(")) {
		goto params_skip;
	}

	tok = next_tok(tokens);
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		plain_type type;
		token *name;

		if (tok->type == T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "the parameter name comes first - the type "
				 "is "
				 "located after a colon like this: `%s: "
				 "%.*s`",
				 plain_type_example_varname(
				     plain_type_from(tok->value, tok->len)),
				 tok->len, tok->value);
		}

		if (tok->type != T_IDENT) {
			error_at(tokens->source->content, tok->value,
				 "missing parameter name");
		}

		name = tok;
		tok = next_tok(tokens);

		if (tok->type == T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "you missed a colon between the name and "
				 "type, try `%.*s: %.*s`",
				 name->len, name->value, tok->len, tok->value);
		}

		if (!TOK_IS(tok, T_PUNCT, ":")) {
			error_at(tokens->source->content, name->value,
				 "the `%.*s` parameter is missing a "
				 "type, try "
				 "`%.*s: i32`",
				 name->len, name->value, name->len,
				 name->value);
		}

		tok = next_tok(tokens);

		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "unknown type `%.*s`", tok->len, tok->value);
		}

		type = plain_type_from(tok->value, tok->len);
		tok = next_tok(tokens);

		if (!TOK_IS(tok, T_PUNCT, ",") && !TOK_IS(tok, T_PUNCT, ")")) {
			error_at(tokens->source->content, tok->value,
				 "unexpected token, this should be either a "
				 "comma `,` or a closing parenthesis `)`");
		}

		fn_add_param(data, name->value, name->len, type);

		if (TOK_IS(tok, T_PUNCT, ","))
			tok = next_tok(tokens);
	}

	/* return type (optional) */
	tok = next_tok(tokens);
params_skip:
	return_type_tok = NULL;
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = next_tok(tokens);

		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "expected return type, got `%.*s`", tok->len,
				 tok->value);
			return ERR_SYNTAX;
		}

		data->return_type = plain_type_from(tok->value, tok->len);
		return_type_tok = tok;
		tok = next_tok(tokens);
	}

	/* opening & closing braces */
	while (tok->type == T_NEWLINE)
		tok = next_tok(tokens);

	if (!TOK_IS(tok, T_PUNCT, "{")) {
		error_at(tokens->source->content, tok->value,
			 "missing opening brace for `%s`", data->name);
		return ERR_SYNTAX;
	}

	int brace_level = 1;
	while (brace_level != 0) {
		tok = next_tok(tokens);

		if (tok->type == T_END) {
			if (brace_level == 1)
				error_at(tokens->source->content, tok->value,
					 "missing a closing brace");
			else
				error_at(tokens->source->content, tok->value,
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
			parse_assign(node, module, data, tokens, tok);
		else if (is_var_decl(tokens, tok))
			parse_var_decl(node, data, tokens, tok);
		else if (is_call(tokens, tok)) {
			parse_call(node, module, tokens, tok);
		} else if (TOK_IS(tok, T_KEYWORD, "ret"))
			parse_return(node, module, tokens, tok);
		else
			error_at(tokens->source->content, tok->value,
				 "unparsable");
	}

	/* always add a return statement */
	if (!fn_has_return(node)) {
		if (data->return_type != PT_NULL) {
			error_at(tokens->source->content, tok->value,
				 "missing return statement for %s", data->name);
		}

		expr_t *ret_expr = expr_add_child(node);
		value_expr_t *val = calloc(1, sizeof(*val));

		ret_expr->type = E_RETURN;
		ret_expr->data_free = (expr_free_handle) value_expr_free;
		ret_expr->data = val;
		val->type = VE_NULL;
		val->return_type = PT_NULL;
	}

	if (!strcmp(data->name, "main")) {
		if (data->n_params) {
			error_at(tokens->source->content, name->value,
				 "the main function does not take any "
				 "arguments");
		}

		if (data->return_type != PT_NULL) {
			error_at(tokens->source->content,
				 return_type_tok->value,
				 "the main function cannot return "
				 "`%.*s`, as it "
				 "always returns `null`",
				 return_type_tok->len, return_type_tok->value);
		}
	}

	return 0;
}

expr_t *parse(token_list *tokens, const char *module_id)
{
	token *current = next_tok(tokens);
	expr_t *module = calloc(sizeof(*module), 1);
	mod_expr_t *data;
	char *content = tokens->source->content;

	data = calloc(1, sizeof(mod_expr_t));
	data->name = strdup(module_id);
	data->source_name = strdup(tokens->source->path);

	module->type = E_MODULE;
	module->data = data;
	module->data_free = (expr_free_handle) mod_expr_free;

	while ((current = next_tok(tokens)) && current->type != T_END) {
		/* top-level: function decl */
		if (current->type == T_KEYWORD
		    && !strncmp(current->value, "fn", current->len)) {
			if (parse_fn(tokens, module))
				goto err;
		} else if (current->type == T_NEWLINE) {
			continue;
		} else {
			error_at(content, current->value,
				 "only functions are allowed at the "
				 "top-level");
			goto err;
		}
	}

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

static const char *expr_typename(expr_type type)
{
	static const char *names[] = {"SKIP",   "MODULE",  "FUNCTION", "CALL",
				      "RETURN", "VARDECL", "ASSIGN",   "VALUE"};
	static const int n_names = sizeof(names) / sizeof(*names);

	if (type >= 0 && type < n_names)
		return names[type];
	return "<EXPR>";
}

char *fn_str_signature(fn_expr_t *func)
{
	char *sig = calloc(1024, 1);
	char buf[64];
	memset(sig, 0, 1024);

	snprintf(sig, 1024, "%s(", func->name);

	for (int i = 0; i < func->n_params - 1; i++) {
		snprintf(buf, 64, "%s: %s, ", func->params[i]->name,
			 plain_type_name(func->params[i]->type));
		strcat(sig, buf);
	}

	if (func->n_params > 0) {
		snprintf(
		    buf, 64, "%s: %s", func->params[func->n_params - 1]->name,
		    plain_type_name(func->params[func->n_params - 1]->type));
		strcat(sig, buf);
	}

	snprintf(buf, 64, "): %s", plain_type_name(func->return_type));
	strcat(sig, buf);

	return sig;
}

static const char *expr_info(expr_t *expr)
{
	static char info[512];
	assign_expr_t *var;
	char *tmp;

	switch (expr->type) {
	case E_MODULE:
		snprintf(info, 512, "%s", E_AS_MOD(expr->data)->name);
		break;
	case E_FUNCTION:
		tmp = fn_str_signature(E_AS_FN(expr->data));
		snprintf(info, 512, "%s", tmp);
		free(tmp);
		break;
	case E_VARDECL:
		snprintf(info, 512, "%s %s",
			 plain_type_name(E_AS_VDECL(expr->data)->type),
			 E_AS_VDECL(expr->data)->name);
		break;
	case E_ASSIGN:
		var = E_AS_ASS(expr->data);
		snprintf(info, 512, "%s = %s", var->name,
			 plain_type_name(var->value->return_type));
		break;
	case E_RETURN:
		snprintf(info, 512, "%s %s",
			 plain_type_name(E_AS_VAL(expr->data)->return_type),
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

	return info;
}

static void expr_print_level(expr_t *expr, int level, bool with_next);

static void expr_print_value_expr(value_expr_t *val, int level)
{
	char *lit_str;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	if (val->type == VE_NULL) {
		printf("literal: null\n");
	} else if (val->type == VE_REF) {
		printf("ref: `%s`\n", val->name);
	} else if (val->type == VE_LIT) {
		lit_str = stringify_literal(val->literal);
		printf("literal: %s %s\n", plain_type_name(val->literal->type),
		       lit_str);
		free(lit_str);
	} else if (val->type == VE_CALL) {
		printf("call: `%s` n_args=%d\n", val->call->name,
		       val->call->n_args);
		for (int i = 0; i < val->call->n_args; i++)
			expr_print_value_expr(val->call->args[i], level + 1);
	} else {
		printf("op: %s\n", value_expr_type_name(val->type));
		if (val->left)
			expr_print_value_expr(val->left, level + 1);
		if (val->right)
			expr_print_value_expr(val->right, level + 1);
	}
}

static void expr_print_level(expr_t *expr, int level, bool with_next)
{
	expr_t *walker;

	for (int i = 0; i < level; i++)
		fputs("  ", stdout);

	printf("%s %s\n", expr_typename(expr->type), expr_info(expr));

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
	plain_type t = literal->type;

	switch (t) {
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
	if (literal->type == PT_I32) {
		char *buf = malloc(16);
		snprintf(buf, 16, "%d", literal->v_i32);
		return buf;
	}

	if (literal->type == PT_STR)
		return strndup(literal->v_str.ptr, literal->v_str.len);

	if (literal->type == PT_NULL) {
		char *buf = malloc(5);
		strcpy(buf, "null");
		return buf;
	}

	return NULL;
}

const char *value_expr_type_name(value_expr_type t)
{
	static const char *names[] = {"NULL", "REF", "LIT", "CALL",
				      "ADD",  "SUB", "MUL", "DIV"};
	static const int n_names = sizeof(names) / sizeof(*names);

	if (t >= 0 && t < n_names)
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
	static const int n_ops = sizeof(ops) / sizeof(*ops);

	for (int i = 0; i < n_ops; i++) {
		if (!strncmp(op->value, ops[i].val, op->len))
			return ops[i].type;
	}

	return VE_NULL;
}
