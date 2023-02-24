/* parser.c - parse a token list into an AST
   Copyright (c) 2023 mini-rose */

#include <ctype.h>
#include <nxg/cc/alloc.h>
#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/cc/type.h>
#include <nxg/utils/error.h>
#include <nxg/utils/utils.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parse_block(settings_t *settings, expr_t *parent, expr_t *module,
			fn_expr_t *fn, expr_t *node, token_list *tokens,
			token *tok);
static void parse_fn_params(expr_t *module, fn_expr_t *decl, token_list *tokens,
			    token *tok);

token *index_tok(token_list *list, int index)
{
	static token end_token = {.type = T_END, .value = "", .len = 0};

	if (index >= list->length)
		return &end_token;

	if (index == 0)
		index = 1;

	return list->tokens[index - 1];
}

token *next_tok(token_list *list)
{
	return index_tok(list, list->iter++);
}

token *after_tok(token_list *list, token *from)
{
	for (int i = 0; i < list->length; i++) {
		if (list->tokens[i] == from)
			return list->tokens[i + 1];
	}

	return index_tok(list, list->length);
}

token *before_tok(token_list *list, token *from)
{
	for (int i = 0; i < list->length; i++) {
		if (list->tokens[i] == from)
			return list->tokens[i - 1];
	}

	return index_tok(list, list->length);
}

static expr_t *expr_add_next(expr_t *expr)
{
	expr_t *node = slab_alloc(sizeof(*node));

	while (expr->next)
		expr = expr->next;

	expr->next = node;
	return node;
}

expr_t *expr_add_child(expr_t *parent)
{
	if (parent->child)
		return expr_add_next(parent->child);

	expr_t *node = slab_alloc(sizeof(*node));
	parent->child = node;
	return node;
}

var_decl_expr_t *node_resolve_local_touch(expr_t *node, const char *name,
					  int len, bool touch)
{
	if (len == 0)
		len = strlen(name);

	if (node->type == E_FUNCTION) {
		fn_expr_t *fn = node->data;

		for (int i = 0; i < fn->n_params; i++) {
			if (strlen(fn->params[i]->name) != len)
				continue;

			if (!strncmp(fn->params[i]->name, name, len)) {
				if (touch)
					fn->params[i]->used = true;
				return fn->params[i];
			}
		}

		for (int i = 0; i < fn->n_locals; i++) {
			if (strlen(fn->locals[i]->name) != len)
				continue;

			if (!strncmp(fn->locals[i]->name, name, len)) {
				if (touch)
					fn->locals[i]->used = true;
				return fn->locals[i];
			}
		}
	} else if (node->type == E_BLOCK) {
		block_expr_t *block = node->data;

		for (int i = 0; i < block->n_locals; i++) {
			if (strlen(block->locals[i]->name) != len)
				continue;

			if (!strncmp(block->locals[i]->name, name, len)) {
				if (touch)
					block->locals[i]->used = true;
				return block->locals[i];
			}
		}

		/* If we don't find anything in our local up, propage the
		   request up. */
		return node_resolve_local_touch(block->parent, name, len,
						touch);
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

static void parse_string_literal(sized_string_t *val, token *tok)
{
	char *buf = slab_alloc(tok->len + 1);
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

void parse_literal(value_expr_t *node, token_list *tokens, token *tok)
{
	node->type = VE_LIT;

	/* string */
	if (tok->type == T_STRING) {
		node->literal = slab_alloc(sizeof(*node->literal));
		node->return_type = type_build_str();
		node->literal->type = type_build_str();
		parse_string_literal(&node->literal->v_str, tok);
		return;
	}

	if (tok->type == T_NUMBER) {
		node->literal = slab_alloc(sizeof(*node->literal));
		char *tmp = slab_strndup(tok->value, tok->len);

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

		return;
	}

	if (tok->type == T_TRUE || tok->type == T_FALSE) {
		node->literal = slab_alloc(sizeof(*node->literal));
		node->return_type = type_new_plain(PT_BOOL);
		node->literal->type = type_new_plain(PT_BOOL);
		node->literal->v_bool = (tok->type == T_TRUE) ? true : false;
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

static void parse_type_err(token_list *tokens, token *tok)
{
	char *fix = NULL;
	char *type = slab_strndup(tok->value, tok->len);

	for (int i = 0; i < tok->len; i++)
		type[i] = tolower(type[i]);

	if (!strcmp("int", type))
		fix = "i32";
	if (!strcmp("string", type))
		fix = "str";
	if (!strcmp("long", type))
		fix = "i64";

	if (!fix)
		error_at(tokens->source, tok->value, tok->len, "unknown type");
	error_at_with_fix(tokens->source, tok->value, tok->len, fix,
			  "unknown type, did you mean to use a %s?", fix);
}

type_t *parse_type(expr_t *context, token_list *tokens, token *tok)
{
	type_t *ty = NULL;

	if (context->type != E_MODULE)
		error("parse_type requires E_MODULE context");

	if (TOK_IS(tok, T_PUNCT, "&")) {
		tok = next_tok(tokens);
		ty = type_new_null();
		ty->kind = TY_POINTER;
		ty->v_base = parse_type(context, tokens, tok);
		return ty;
	}

	if (tok->type == T_IDENT) {
		/* Our special case: the string */
		if (!strncmp(tok->value, "str", tok->len)) {
			ty = type_build_str();
			goto array_part;
		}

		char *name = slab_strndup(tok->value, tok->len);
		ty = module_find_named_type(context, name);

		if (!ty)
			parse_type_err(tokens, tok);

		goto array_part;
	}

	if (tok->type == T_DATATYPE)
		ty = type_from_sized_string(tok->value, tok->len);

	if (!ty) {
		error_at(tokens->source, tok->value, tok->len,
			 "failed to parse type");
	}

array_part:
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
		array_ty->kind = TY_ARRAY;
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

static void fn_add_local_var(fn_expr_t *func, var_decl_expr_t *var)
{
	func->locals = realloc_ptr_array(func->locals, func->n_locals + 1);
	func->locals[func->n_locals++] = var;
}

static void block_add_local_var(block_expr_t *block, var_decl_expr_t *var)
{
	block->locals = realloc_ptr_array(block->locals, block->n_locals + 1);
	block->locals[block->n_locals++] = var;
}

/**
 * name: type
 */
static err_t parse_var_decl(expr_t *parent, expr_t *module, token_list *tokens,
			    token *tok)
{
	var_decl_expr_t *data;
	expr_t *node;

	if (node_has_named_local(parent, tok->value, tok->len)) {
		error_at(tokens->source, tok->value, tok->len,
			 "a variable named `%.*s` already has been "
			 "declared in this scope",
			 tok->len, tok->value);
	}

	data = slab_alloc(sizeof(*data));
	data->name = slab_strndup(tok->value, tok->len);
	data->decl_location = tok;

	tok = next_tok(tokens);
	tok = next_tok(tokens);

	if (!is_type(tokens, tok)) {
		error_at(tokens->source, tok->value, tok->len,
			 "expected type in variable declaration");
	}

	data->type = parse_type(module, tokens, tok);

	node = expr_add_child(parent);
	node->type = E_VARDECL;
	node->data = data;

	if (parent->type == E_FUNCTION)
		fn_add_local_var(parent->data, data);
	else if (parent->type == E_BLOCK)
		block_add_local_var(parent->data, data);

	return ERR_OK;
}

/**
 * name[: type] = value
 */
static err_t parse_assign(settings_t *settings, expr_t *parent, expr_t *mod,
			  fn_expr_t *fn, token_list *tokens, token *tok)
{
	assign_expr_t *data;
	expr_t *node;
	token *name;
	token *dot_tok = NULL;
	token *member_tok = NULL;
	bool deref = false;
	bool guess_type = false;
	bool member_access = false;
	expr_t *guess_decl;

	name = tok;

	if (tok->type == T_MUL) {
		deref = true;
		tok = next_tok(tokens);
		name = tok;
	}

	/* defined variable type, meaning we declare a new variable */
	if (is_var_decl(tokens, tok)) {
		parse_var_decl(parent, mod, tokens, tok);
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
		guess_decl->data = slab_alloc(sizeof(var_decl_expr_t));
		E_AS_VDECL(guess_decl->data)->name =
		    slab_strndup(name->value, name->len);
		E_AS_VDECL(guess_decl->data)->decl_location = name;
	}

	node = expr_add_child(parent);
	data = slab_alloc(sizeof(*data));
	data->to = slab_alloc(sizeof(*data->to));
	data->to->type = deref ? VE_DEREF : VE_REF;
	data->to->name = slab_strndup(name->value, name->len);

	node->type = E_ASSIGN;
	node->data = data;

	tok = next_tok(tokens);

	/* Assign to member */
	if (tok->type == T_DOT) {
		member_access = true;
		dot_tok = tok;

		tok = next_tok(tokens);
		if (tok->type != T_IDENT) {
			error_at(
			    tokens->source, tok->value, tok->len,
			    "expected field name after member access operator");
		}

		member_tok = tok;
		data->to->member = slab_strndup(tok->value, tok->len);
		data->to->type = data->to->type == VE_REF ? VE_MREF : VE_MDEREF;

		tok = next_tok(tokens);
	}

	/* Parse value */
	tok = next_tok(tokens);
	data->value = slab_alloc(sizeof(*data->value));
	data->value =
	    parse_value_expr(settings, parent, mod, data->value, tokens, tok);

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

		type_t *local_type = NULL;

		/* object.field */
		if (member_access) {
			type_t *o_type = local->type;
			if (o_type->kind == TY_POINTER)
				o_type = o_type->v_base;

			if (o_type->kind != TY_OBJECT) {
				error_at(tokens->source, dot_tok->value,
					 dot_tok->len,
					 "cannot access member of non-object "
					 "variable");
			}

			local_type = type_of_member(o_type, data->to->member);

			if (local_type->kind == TY_NULL) {
				error_at(tokens->source, member_tok->value,
					 member_tok->len, "unknown field of %s",
					 o_type->v_object->name);
			}

		} else {
			local_type = type_copy(local->type);
		}

		/* *var */
		if (deref) {
			if (local->type->kind != TY_POINTER) {
				error_at(
				    tokens->source, name->value, name->len,
				    "cannot dereference a non-pointer type");
			}

			data->to->return_type = type_copy(local_type->v_base);
		} else {
			data->to->return_type = type_copy(local_type);
		}
	}

	if (!type_cmp(data->to->return_type, data->value->return_type)) {
		if (data->to->return_type->kind == TY_POINTER) {
			if (type_cmp(data->to->return_type->v_base,
				     data->value->return_type)) {
				char fix[128];
				snprintf(fix, 128, "*%s", data->to->name);
				error_at_with_fix(
				    tokens->source, name->value, name->len, fix,
				    "mismatched types in assignment: did you "
				    "mean to dereference the pointer?");
			}
		}

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
static err_t parse_return(settings_t *settings, expr_t *parent, expr_t *mod,
			  token_list *tokens, token *tok)
{
	type_t *return_type;
	value_expr_t *data;
	expr_t *node;

	node = expr_add_child(parent);
	data = slab_alloc(sizeof(*data));

	node->type = E_RETURN;
	node->data = data;

	return_type = type_new_null();
	if (parent->type == E_FUNCTION) {
		return_type = type_copy(E_AS_FN(parent->data)->return_type);
	}

	tok = next_tok(tokens);
	if (tok->type == T_NEWLINE && return_type->kind != TY_NULL) {
		error_at(tokens->source, tok->value, tok->len,
			 "missing return value for function that "
			 "returns `%s`",
			 type_name(return_type));
	}

	if (tok->type != T_NEWLINE && return_type->kind == TY_NULL) {
		error_at(tokens->source, tok->value, tok->len,
			 "cannot return value, because `%s` returns null",
			 E_AS_FN(parent->data)->name);
	}

	/* return without a value */
	if (return_type->kind == TY_NULL) {
		data->type = VE_NULL;
		return ERR_OK;
	}

	/* return with a value */
	data = parse_value_expr(settings, parent, mod, data, tokens, tok);
	node->data = data;

	if (!type_cmp(data->return_type, E_AS_FN(parent->data)->return_type)) {
		error_at(tokens->source, tok->value, tok->len,
			 "mismatched return type: expression returns "
			 "%s, while "
			 "the function returns %s",
			 type_name(data->return_type),
			 type_name(E_AS_FN(parent->data)->return_type));
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

/**
 * ([param[, param]...])
 */
static void parse_fn_params(expr_t *module, fn_expr_t *decl, token_list *tokens,
			    token *tok)
{
	tok = next_tok(tokens);
	while (tok->type != T_RPAREN) {
		type_t *type;
		token *name;

		if (tok->type == T_DATATYPE || TOK_IS(tok, T_PUNCT, "&")) {
			char fix[128];
			int errlen;
			type = parse_type(module, tokens, tok);
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
			type = parse_type(module, tokens, tok);

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

		type = parse_type(module, tokens, tok);
		tok = next_tok(tokens);

		if (tok->type != T_COMMA && tok->type != T_RPAREN) {
			error_at_with_fix(tokens->source, tok->value, tok->len,
					  ", or )", "unexpected token");
		}

		fn_add_param(decl, name->value, name->len, type);

		if (tok->type == T_COMMA)
			tok = next_tok(tokens);
	}
}

/**
 * fn name([param[, param]...])[-> return_type]
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
	decl->name = slab_strndup(tok->value, tok->len);

	/* parameters (optional) */
	tok = next_tok(tokens);
	if (tok->type != T_LPAREN)
		goto params_skip;

	parse_fn_params(module, decl, tokens, tok);

	/* return type (optional) */
	tok = next_tok(tokens);
	return_type_tok = NULL;

params_skip:
	if (tok->type == T_ARROW) {
		tok = next_tok(tokens);

		if (!is_type(tokens, tok)) {
			error_at(tokens->source, tok->value, tok->len,
				 "expected return type, got `%.*s`", tok->len,
				 tok->value);
		}

		decl->return_type = parse_type(module, tokens, tok);
		return_type_tok = tok;
		tok = next_tok(tokens);
	}

	if (!strcmp(decl->name, "main")) {
		if (decl->n_params) {
			error_at(tokens->source, name->value, name->len,
				 "the main function does not take any "
				 "arguments");
		}

		if (decl->return_type->kind != TY_NULL) {
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
 * (expr) ? { ... }
 */
static void parse_condition(settings_t *settings, expr_t *parent,
			    expr_t *module, fn_expr_t *fn, token_list *tokens,
			    token *tok)
{
	condition_expr_t *cond;
	expr_t *node;
	token *start_tok;

	tok = next_tok(tokens);
	node = expr_add_child(parent);
	cond = slab_alloc(sizeof(*cond));

	start_tok = tok;

	node->type = E_CONDITION;
	node->data = cond;
	cond->cond = slab_alloc(sizeof(*cond->cond));

	/* Parse the condition, and ensure it returns bool. */
	cond->cond =
	    parse_value_expr(settings, parent, module, cond->cond, tokens, tok);

	tok = index_tok(tokens, tokens->iter - 1);
	if (cond->cond->type == VE_NULL) {
		error_at(tokens->source, start_tok->value,
			 tok->value - start_tok->value + 1,
			 "this not a valid condition");
	}

	if (cond->cond->return_type->kind == TY_PLAIN) {
		if (cond->cond->return_type->v_plain == PT_BOOL)
			goto skip_bool_check;
	}

	error_at(tokens->source, start_tok->value,
		 tok->value - start_tok->value + 1,
		 "expression does not return a boolean value");

skip_bool_check:

	tok = next_tok(tokens);
	if (tok->type != T_RPAREN) {
		error_at_with_fix(
		    tokens->source, tok->value, tok->len, ")",
		    "expected closing parenthesis after condition");
	}

	tok = next_tok(tokens);
	if (!TOK_IS(tok, T_PUNCT, "?")) {
		error_at_with_fix(tokens->source, tok->value, tok->len, "?",
				  "expected `?` sign after condition");
	}

	/* Now, we can either have a block '{ ... }' or a single value in the
	   truth block. */

	cond->if_block = slab_alloc(sizeof(*cond->if_block));

	tok = next_tok(tokens);
	if (tok->type == T_LBRACE) {
		/* Block */
		cond->if_block->type = E_BLOCK;
		cond->if_block->data = slab_alloc(sizeof(block_expr_t));

		E_AS_BLOCK(cond->if_block->data)->parent = parent;

		parse_block(settings, parent, module, fn, cond->if_block,
			    tokens, tok);

		if (!cond->if_block->child) {
			if (settings->warn_empty_block) {
				warning_at(tokens->source, tok->value, tok->len,
					   "empty block will be removed by the "
					   "compiler");
			}
			cond->if_block->type = E_SKIP;
		}

	} else {
		/* Single value */
		cond->if_block->type = E_VALUE;
		cond->if_block->data = slab_alloc(sizeof(value_expr_t));

		cond->if_block->data =
		    parse_value_expr(settings, parent, module,
				     cond->if_block->data, tokens, tok);
	}

	/* Have a peek if we have an else block. */
	tok = index_tok(tokens, tokens->iter);
	if (!TOK_IS(tok, T_PUNCT, ":")) {
		return;
	}

	/* else block ': { ... }' */
	cond->else_block = slab_alloc(sizeof(*cond->else_block));

	tok = next_tok(tokens);
	tok = next_tok(tokens);
	if (tok->type == T_LBRACE) {
		/* Block */
		cond->else_block->type = E_BLOCK;
		cond->else_block->data = slab_alloc(sizeof(block_expr_t));

		E_AS_BLOCK(cond->else_block->data)->parent = parent;

		parse_block(settings, parent, module, fn, cond->else_block,
			    tokens, tok);

		if (!cond->else_block->child) {
			if (settings->warn_empty_block) {
				warning_at(tokens->source, tok->value, tok->len,
					   "empty block will be removed by the "
					   "compiler");
			}
			cond->else_block->type = E_SKIP;
		}

	} else {
		/* Single value */
		cond->else_block->type = E_VALUE;
		cond->else_block->data = slab_alloc(sizeof(value_expr_t));

		cond->else_block->data =
		    parse_value_expr(settings, parent, module,
				     cond->else_block->data, tokens, tok);
	}
}

/**
 * {
 *   [statement]...
 * }
 */
static void parse_block(settings_t *settings, expr_t *parent, expr_t *module,
			fn_expr_t *fn, expr_t *node, token_list *tokens,
			token *tok)
{
	int brace_level = 1;

	if (tok->type != T_LBRACE) {
		error_at(tokens->source, tok->value, tok->len,
			 "missing opening brace for block");
	}

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

		if (tok->type == T_LBRACE) {
			brace_level++;
			continue;
		}

		if (tok->type == T_RBRACE) {
			brace_level--;
			continue;
		}

		/* Statements inside a block */

		if (is_var_assign(tokens, tok))
			parse_assign(settings, node, module, fn, tokens, tok);
		else if (is_var_decl(tokens, tok))
			parse_var_decl(node, module, tokens, tok);
		else if (is_call(tokens, tok))
			parse_call(settings, node, module, tokens, tok);
		else if (is_member_call(tokens, tok))
			parse_member_call(settings, node, module, tokens, tok);
		else if (tok->type == T_LPAREN)
			parse_condition(settings, node, module, fn, tokens,
					tok);
		else if (TOK_IS(tok, T_KEYWORD, "ret"))
			parse_return(settings, node, module, tokens, tok);
		else if (TOK_IS(tok, T_IDENT, "return"))
			error_at_with_fix(tokens->source, tok->value, tok->len,
					  "ret", "did you mean `ret`?");
		else
			error_at(tokens->source, tok->value, tok->len,
				 "unparsable");
	}
}

static err_t parse_fn_body(settings_t *settings, expr_t *module, expr_t *node,
			   fn_expr_t *decl, token_list *tokens)
{
	token *tok;

	node->type = E_FUNCTION;
	node->data = decl;

	tok = next_tok(tokens);

	/* opening & closing braces */
	while (tok->type == T_NEWLINE)
		tok = next_tok(tokens);

	parse_block(settings, node, module, decl, node, tokens, tok);

	/* always add a return statement */
	if (!fn_has_return(node)) {
		if (decl->return_type->kind != TY_NULL) {
			error_at(tokens->source, tok->value, tok->len,
				 "missing return statement for %s", decl->name);
		}

		expr_t *ret_expr = expr_add_child(node);
		value_expr_t *val = slab_alloc(sizeof(*val));

		ret_expr->type = E_RETURN;
		ret_expr->data = val;
		val->type = VE_NULL;
	}

	if (settings->warn_unused)
		fn_warn_unused(tokens->source, node);

	return 0;
}

typedef struct
{
	int tok_index;
	fn_expr_t *decl;
} fn_pos_t;

typedef struct
{
	fn_pos_t **pos;
	int n;
} fn_positions_t;

static fn_pos_t *acquire_fn_pos(fn_positions_t *postions)
{
	postions->pos = realloc_ptr_array(postions->pos, postions->n + 1);
	postions->pos[postions->n] = slab_alloc(sizeof(fn_pos_t));
	return postions->pos[postions->n++];
}

static void skip_block(token_list *tokens, token *tok)
{
	int depth = 0;

	do {
		if (tok->type == T_LBRACE)
			depth++;
		if (tok->type == T_RBRACE)
			depth--;
		tok = next_tok(tokens);
	} while (depth);
}

static void parse_single_use(settings_t *settings, expr_t *module,
			     token_list *tokens, token *tok)
{
	token *tmp_tok;
	token *start, *end;
	char *path;
	int offset = 0;

	start = tok;

	while ((end = index_tok(tokens, tokens->iter + offset))->type
	       != T_NEWLINE)
		offset++;

	int n = end->value - start->value;

	if (tok->type == T_STRING) {
		path = slab_strndup(tok->value, tok->len);
		if (!module_import(settings, module, path)) {
			error_at(tokens->source, start->value - 1, n + 1,
				 "cannot find module");
		}

		return;
	}

	if (tok->type != T_IDENT) {
		error_at(tokens->source, tok->value, tok->len,
			 "expected module name or path");
	}

	/* Collect path */
	path = slab_alloc(512);
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

		if (tok->type != T_DOT) {
			error_at_with_fix(
			    tokens->source, tok->value, tok->len, ".",
			    "expected dot seperator after module name");
		}

		/* Warn the user about dots at the end. */
		tmp_tok = index_tok(tokens, tokens->iter);
		if (tmp_tok->type == T_NEWLINE && settings->warn_random) {
			warning_at(tokens->source, tmp_tok->value - 1,
				   tmp_tok->len, "unnecessary dot");
		}

	} while ((tok = next_tok(tokens))->type != T_NEWLINE);

	if (!module_std_import(settings, module, path)) {
		error_at(tokens->source, start->value, n,
			 "cannot find module in standard library");
	}
}

static void parse_use(settings_t *settings, expr_t *module, token_list *tokens,
		      token *tok)
{
	tok = next_tok(tokens);


	if (tok->type == T_IDENT || tok->type == T_STRING) {
		parse_single_use(settings, module, tokens, tok);
	} else if (tok->type == T_LBRACE) {
		while ((tok = next_tok(tokens))->type != T_RBRACE) {
			if (tok->type == T_NEWLINE)
				continue;

			parse_single_use(settings, module, tokens, tok);

			while (tok->type != T_NEWLINE)
				tok = next_tok(tokens);
		}
	}
}

static void parse_object_fields(settings_t *settings, expr_t *module,
				type_t *ty, token_list *tokens, token *tok)
{
	char *ident;
	type_t *field;

	while (tok->type == T_NEWLINE)
		tok = next_tok(tokens);

	while (tok->type != T_RBRACE) {
		/* field name */
		if (tok->type != T_IDENT) {
			error_at(tokens->source, tok->value, tok->len,
				 "expected field name");
		}

		ident = slab_strndup(tok->value, tok->len);

		tok = next_tok(tokens);
		if (!TOK_IS(tok, T_PUNCT, ":")) {
			error_at(tokens->source, tok->value, tok->len,
				 "expected colon seperating the name and type");
		}

		tok = next_tok(tokens);

		if (TOK_IS(tok, T_KEYWORD, "fn")) {
			tok = next_tok(tokens);

			/* Type method */
			expr_t *method = type_object_add_method(ty->v_object);
			fn_expr_t *func = slab_alloc(sizeof(*func));

			method->type = E_FUNCTION;
			method->data = func;
			func->name = ident;
			func->object = ty->v_object;

			if (tok->type == T_LPAREN)
				parse_fn_params(module, func, tokens, tok);

			/* Check if self is present and is the same type as the
			 * &T. */
			bool self_correct_type = type_cmp(func->params[0]->type,
							  type_pointer_of(ty));
			char *fix = slab_alloc(256);
			snprintf(fix, 256, "self: &%s", ty->v_object->name);

			if (func->n_params == 0 || !self_correct_type) {
				error_at_with_fix(
				    tokens->source, tok->value, tok->len, fix,
				    "the first parameter of a method must take "
				    "a reference to this object");
			}

			if (settings->warn_self_name
			    && strcmp(func->params[0]->name, "self")) {
				tok = after_tok(tokens, tok);
				warning_at(tokens->source, tok->value, tok->len,
					   "the reference to the object should "
					   "be named `self`");
			}

			if (tok->type == T_ARROW) {
				tok = next_tok(tokens);
				func->return_type =
				    parse_type(module, tokens, tok);
			} else {
				func->return_type = type_new_null();
			}

			tok = index_tok(tokens, tokens->iter);
			parse_fn_body(settings, module, method, func, tokens);

		} else if (is_type(tokens, tok)) {

			/* Regular type field */
			field = parse_type(module, tokens, tok);
			type_object_add_field(ty->v_object, ident, field);

		} else {
			highlight_t hi = highlight_value(tokens, tok);
			error_at(tokens->source, hi.value, hi.len,
				 "expected type or method");
		}

		tok = next_tok(tokens);
		while (tok->type == T_NEWLINE)
			tok = next_tok(tokens);
	}
}

/**
 * type ::= "type" ident = ident
 *      ::= "type" ident { [ident ident newline]... }
 */
static void parse_type_decl(settings_t *settings, expr_t *module,
			    token_list *tokens, token *tok)
{
	type_t *ty;
	token *name;

	/* name */
	if ((tok = next_tok(tokens))->type != T_IDENT) {
		error_at(tokens->source, tok->value, tok->len,
			 "expected type name");
	}

	name = tok;
	tok = next_tok(tokens);

	/* = or { */
	if (tok->type == T_ASS) {

		/* Type alias */
		ty = module_add_type_decl(module);

		tok = next_tok(tokens);
		ty->kind = TY_ALIAS;
		ty->alias = slab_strndup(name->value, name->len);
		ty->v_base = parse_type(module, tokens, tok);

	} else if (tok->type == T_LBRACE) {

		/* Object type */
		ty = module_add_type_decl(module);
		ty->kind = TY_OBJECT;
		ty->v_object = slab_alloc(sizeof(*ty->v_object));
		ty->v_object->name = slab_strndup(name->value, name->len);

		tok = next_tok(tokens);
		parse_object_fields(settings, module, ty, tokens, tok);

	} else {
		error_at(tokens->source, tok->value, tok->len,
			 "expected `=` for alias or `{` for structure type");
	}
}

expr_t *parse(expr_t *parent, expr_t *module, settings_t *settings,
	      token_list *tokens, const char *module_id)
{
	token *current = next_tok(tokens);
	mod_expr_t *data = module->data;
	fn_positions_t fn_pos = {0};

	data->name = slab_strdup(module_id);
	data->source_name = slab_strdup(tokens->source->path);

	module->type = E_MODULE;
	module->data = data;

	if (parent) {
		data->std_modules = E_AS_MOD(parent->data)->std_modules;
		data->c_objects = E_AS_MOD(parent->data)->c_objects;
	}

	/*
	 * In order to support overloading & use-before-declare we need
	 * to parse the declarations before the contents.
	 */

	while ((current = next_tok(tokens)) && current->type != T_END) {
		/* top-level: function decl */

		if (TOK_IS(current, T_KEYWORD, "fn")) {

			/* parse only the fn declaration, leave the rest */
			fn_pos_t *pos = acquire_fn_pos(&fn_pos);
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
		} else if (TOK_IS(current, T_KEYWORD, "type")) {
			parse_type_decl(settings, module, tokens, current);
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
	for (int i = 0; i < fn_pos.n; i++) {
		tokens->iter = fn_pos.pos[i]->tok_index;

		expr_t *func = expr_add_child(module);
		parse_fn_body(settings, module, func, fn_pos.pos[i]->decl,
			      tokens);
	}

err:
	return module;
}

char *fn_str_signature(fn_expr_t *func, bool with_colors)
{
	char *sig = slab_alloc(1024);
	char buf[64];
	char *ty_str;
	memset(sig, 0, 1024);

	snprintf(sig, 1024, "%s%s%s(", with_colors ? "\e[94m" : "", func->name,
		 with_colors ? "\e[0m" : "");

	for (int i = 0; i < func->n_params - 1; i++) {
		ty_str = type_name(func->params[i]->type);
		snprintf(buf, 64, "%s%s%s, ", with_colors ? "\e[33m" : "",
			 ty_str, with_colors ? "\e[0m" : "");
		strcat(sig, buf);
	}

	if (func->n_params > 0) {
		ty_str = type_name(func->params[func->n_params - 1]->type);
		snprintf(buf, 64, "%s%s%s", with_colors ? "\e[33m" : "", ty_str,
			 with_colors ? "\e[0m" : "");
		strcat(sig, buf);
	}

	ty_str = type_name(func->return_type);
	snprintf(buf, 64, ") -> %s%s%s", with_colors ? "\e[33m" : "", ty_str,
		 with_colors ? "\e[0m" : "");
	strcat(sig, buf);

	return sig;
}

void literal_default(literal_expr_t *literal)
{
	type_t *t = literal->type;
	memset(literal, 0, sizeof(*literal));
	literal->type = t;
}

char *stringify_literal(literal_expr_t *literal)
{
	if (is_str_type(literal->type))
		return slab_strndup(literal->v_str.ptr, literal->v_str.len);

	if (literal->type->kind != TY_PLAIN)
		error("literal cannot be of non-plain type");

	if (literal->type->v_plain == PT_I32) {
		char *buf = slab_alloc(16);
		snprintf(buf, 16, "%d", literal->v_i32);
		return buf;
	}

	if (literal->type->v_plain == PT_I64) {
		char *buf = slab_alloc(32);
		snprintf(buf, 32, "%ld", literal->v_i64);
		return buf;
	}

	if (literal->type == TY_NULL) {
		char *buf = slab_alloc(5);
		strcpy(buf, "null");
		return buf;
	}

	return NULL;
}

value_expr_type value_expr_type_from_op(token *op)
{
	static const struct
	{
		token_t token;
		value_expr_type type;
	} ops[] = {
	    {T_ADD, VE_ADD}, {T_SUB, VE_SUB}, {T_MUL, VE_MUL}, {T_DIV, VE_DIV}};

	for (int i = 0; i < LEN(ops); i++) {
		if (op->type == ops[i].token)
			return ops[i].type;
	}

	return VE_NULL;
}
