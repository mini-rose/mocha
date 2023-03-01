/* parser/call.c - parse calls
   Copyright (c) 2023 mini-rose */

#include "nxg/cc/type.h"
#include "nxg/cc/use.h"

#include <nxg/cc/alloc.h>
#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/utils/error.h>
#include <nxg/utils/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void call_push_arg(call_expr_t *call, value_expr_t *node)
{
	call->args = realloc_ptr_array(call->args, call->n_args + 1);
	call->args[call->n_args++] = node;
}

static value_expr_t *call_add_arg(call_expr_t *call)
{
	value_expr_t *node = slab_alloc(sizeof(*node));
	call_push_arg(call, node);
	return node;
}

static void call_popfront_arg(call_expr_t *call)
{
	value_expr_t **old_args = call->args;
	int old_n = call->n_args - 1;

	call->args = realloc_ptr_array(NULL, old_n);
	call->n_args = 0;

	for (int i = 0; i < old_n; i++)
		call_push_arg(call, old_args[i + 1]);
}

bool is_builtin_function(token *name)
{
	static const char *builtins[] = {"__builtin_decl",
					 "__builtin_decl_mangled"};

	for (size_t i = 0; i < LEN(builtins); i++) {
		if (!strncmp(builtins[i], name->value, name->len))
			return true;
	}

	return false;
}

static void collect_builtin_decl_arguments(fn_expr_t *decl, token_list *tokens)

{
	token *arg;

	while ((arg = next_tok(tokens))->type != T_NEWLINE) {
		type_t *ty = parse_type(decl->module, tokens, arg);
		fn_add_param(decl, "_", 1, ty);

		arg = next_tok(tokens);
		if (arg->type == T_RPAREN)
			break;

		if (arg->type != T_COMMA) {
			error_at_with_fix(tokens->source, arg->value, arg->len,
					  ",",
					  "expected comma between arguments",
					  arg->len, arg->value);
		}
	}
}

typedef struct
{
	token **tokens;
	int n_tokens;
} arg_tokens_t;

static void add_arg_token(arg_tokens_t *tokens, token *tok)
{
	tokens->tokens =
	    realloc_ptr_array(tokens->tokens, tokens->n_tokens + 1);
	tokens->tokens[tokens->n_tokens++] = tok;
}

err_t parse_builtin_call(expr_t *parent, expr_t *mod, token_list *tokens,
			 token *tok)
{
	/* Built-in calls are not always function calls, they may be
	   "macro-like" functions which turn into something else. */

	token *name, *arg;

	name = tok;

	/* Parse __builtin_decl & __builtin_decl_mangled the same way, just set
	   a different flag for the mangled version. */
	if (!strncmp("__builtin_decl", name->value, name->len)
	    || !strncmp("__builtin_decl_mangled", name->value, name->len)) {
		fn_expr_t *decl = module_add_decl(mod);

		if (!strncmp("__builtin_decl", name->value, name->len))
			decl->flags = FN_NOMANGLE;

		/* function name */
		arg = next_tok(tokens);
		arg = next_tok(tokens);
		if (arg->type != T_STRING) {
			error_at(tokens->source, arg->value, arg->len,
				 "first argument to this builtin must be a "
				 "string with the function name");
		}

		decl->name = slab_strndup(arg->value, arg->len);

		arg = next_tok(tokens);
		if (arg->type != T_COMMA) {
			error_at(tokens->source, arg->value, arg->len,
				 "missing return type argument");
		}

		/* return type */
		arg = next_tok(tokens);
		if (!is_type(tokens, arg)) {
			error_at(tokens->source, arg->value, arg->len,
				 "second argument to this builtin is "
				 "expected to be the return type");
		}

		expr_t *context = parent;
		if (parent->type == E_FUNCTION)
			context = E_AS_FN(parent->data)->module;

		decl->return_type = parse_type(context, tokens, arg);

		arg = next_tok(tokens);
		if (arg->type == T_RPAREN)
			return ERR_WAS_BUILTIN;

		if (arg->type != T_COMMA) {
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

static bool possibly_convert_val(value_expr_t *value, type_t *into)
{
	/* Check if we can maybe convert a literal. */
	if (type_can_be_converted(value->return_type, into)) {
		return convert_int_value(value, into);
	}

	return false;
}

err_t parse_inline_call(settings_t *settings, expr_t *parent, expr_t *mod,
			call_expr_t *data, token_list *tokens, token *tok)
{
	arg_tokens_t arg_tokens = {0};
	token *fn_name_tok;
	value_expr_t *self_arg = NULL;
	bool static_method = false;
	value_expr_t *arg;

	if (is_builtin_function(tok))
		return parse_builtin_call(parent, mod, tokens, tok);

	data->name = slab_strndup(tok->value, tok->len);
	fn_name_tok = tok;

	tok = next_tok(tokens);
	tok = next_tok(tokens);

	/* If this is a method, the first argument is always `self` (unless its
	   static). */
	if (data->object_name) {
		self_arg = call_add_arg(data);
		add_arg_token(&arg_tokens, tok);
	}

	/* Arguments */
	while (tok->type != T_RPAREN) {
		if (is_rvalue(tokens, tok)) {
			arg = slab_alloc(sizeof(*arg));
			add_arg_token(&arg_tokens, tok);
			arg = parse_value_expr(settings, parent, mod, arg,
					       tokens, tok);
			call_push_arg(data, arg);
		} else {
			error_at(tokens->source, tok->value, tok->len,
				 "expected value or variable name, got "
				 "`%.*s`",
				 tok->len, tok->value);
		}

		tok = next_tok(tokens);

		if (tok->type == T_RPAREN)
			break;

		if (tok->type != T_COMMA) {
			error_at_with_fix(
			    tokens->source, tok->value, tok->len, ",",
			    "expected comma seperating the arguments");
		}

		tok = next_tok(tokens);
	}

	fn_candidates_t *resolved;

auto_imported:
	if (data->object_name) {
		var_decl_expr_t *var;
		type_t *obj_type;
		token *o_name;
		type_t *o_type;

		/* Little hack: if we know its a member call, step 2 tokens back
		   to get the name token. */
		o_name = before_tok(tokens, fn_name_tok);
		o_name = before_tok(tokens, o_name);

		/* Get the type of the object */
		var = node_resolve_local(parent, data->object_name, 0);
		if (!var) {
			/* Maybe we want to call a static method? Try getting
			   the type here. */

			o_type = module_find_named_type(mod, data->object_name);
			if (!o_type) {
				char *type_name =
				    strndup(o_name->value, o_name->len);

				if (auto_import(settings, parent, mod,
						type_name)) {
					free(type_name);
					goto auto_imported;
				}

				error_at(tokens->source, o_name->value,
					 o_name->len,
					 "use of an undeclared variable");
			}

			static_method = true;
			obj_type = o_type;
		} else {
			obj_type = var->type;
		}

		if (obj_type->kind == TY_POINTER)
			obj_type = obj_type->v_base;

		if (obj_type->kind != TY_OBJECT) {
			error_at(tokens->source, o_name->value, o_name->len,
				 "non-object types do not have methods");
		}

		/* Add `self` to the call. */
		if (!static_method) {
			self_arg->type =
			    var->type->kind == TY_POINTER ? VE_REF : VE_PTR;
			self_arg->return_type = type_pointer_of(obj_type);
			self_arg->name = slab_strdup(var->name);
		} else {
			/* Remove the implcit `self` argument. */
			call_popfront_arg(data);
		}

		resolved = type_object_find_fn_candidates(obj_type->v_object,
							  data->name);

		/* Method call has a different error */
		if (!resolved->n_candidates) {
			error_at(tokens->source, fn_name_tok->value,
				 fn_name_tok->len,
				 "%s has no method named `%s`",
				 obj_type->v_object->name, data->name);
		}

	} else {
		resolved = module_find_fn_candidates(mod, data->name);
	}

	/* Bare call */
	if (!resolved->n_candidates) {
		if (auto_import(settings, parent, mod, data->name))
			goto auto_imported;

		error_at(tokens->source, fn_name_tok->value, fn_name_tok->len,
			 "no function named `%s` found", data->name);
	}

	bool try_next;

try_matching_types_again:

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

		goto end;
	}

	/* We cannot find any matching overloads, so maybe we can find a literal
	   that can be converted to match the type. */
	for (int i = 0; i < resolved->n_candidates; i++) {
		fn_expr_t *match = resolved->candidate[i];
		try_next = false;

		if (match->n_params != data->n_args)
			continue;

		/* Check for argument types */
		for (int j = 0; j < match->n_params; j++) {
			if (possibly_convert_val(data->args[j],
						 match->params[j]->type)) {
				goto try_matching_types_again;
			}
		}

		if (try_next)
			continue;
	}

	fprintf(stderr,
		"\e[1;91moverload mismatch\e[0m, found \e[92m%d\e[0m potential "
		"candidate(s), but none of them match:\n",
		resolved->n_candidates);
	for (int i = 0; i < resolved->n_candidates; i++) {
		char *sig = fn_str_signature(resolved->candidate[i], true);
		fprintf(stderr, "  %s\n", sig);
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
			if (match->params[j]->type->kind != TY_POINTER)
				continue;

			char *fix;

			if (data->args[j]->type == VE_REF) {
				fix = slab_alloc(64);
				snprintf(fix, 64, "&%.*s",
					 arg_tokens.tokens[j]->len,
					 arg_tokens.tokens[j]->value);

				error_at_with_fix(
				    tokens->source, arg_tokens.tokens[j]->value,
				    arg_tokens.tokens[j]->len, fix,
				    "%s takes a reference to `%s` here",
				    match->name,
				    type_name(match->params[j]->type->v_base));
				continue;
			}

			if (data->args[j]->type == VE_LIT) {
				error_at(tokens->source,
					 arg_tokens.tokens[j]->value,
					 arg_tokens.tokens[j]->len,
					 "cannot take reference of literal, "
					 "create a variable first");
			}

			if (!type_cmp(match->params[j]->type->v_base,
				      data->args[j]->return_type)) {
				continue;
			}

			fix = slab_alloc(64);
			snprintf(fix, 64, "&%.*s", arg_tokens.tokens[j]->len,
				 arg_tokens.tokens[j]->value);

			highlight_t hi =
			    highlight_value(tokens, arg_tokens.tokens[j]);
			error_at_with_fix(
			    tokens->source, hi.value, hi.len, fix,
			    "%s takes a `%s` here, did you mean to "
			    "pass a reference?",
			    data->name, type_name(match->params[j]->type),
			    data->args[j]->name);
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
	/* Common case: print() takes both a string reference and a string copy
	   to support literals. This also allows the user to pass copies when
	   a reference would be the better option. [prefer-ref] */

	if (!strcmp(data->name, "print") && settings->warn_prefer_ref) {
		if (data->n_args == 0)
			goto skip_ref_warn;

		if (!is_str_type(data->args[0]->return_type))
			goto skip_ref_warn;

		if (data->args[0]->type == VE_LIT)
			goto skip_ref_warn;

		token *a = arg_tokens.tokens[0];
		char *fix = slab_alloc(a->len + 2);
		snprintf(fix, a->len + 2, "&%.*s", a->len, a->value);

		warning_at_with_fix(
		    tokens->source, a->value, a->len, fix,
		    "unnecessary copy, you should pass a reference here");
	}

skip_ref_warn:
	return ERR_OK;
}

/**
 * ident([arg, ...])
 */
void parse_call(settings_t *settings, expr_t *parent, expr_t *mod,
		token_list *tokens, token *tok)
{
	expr_t *node;
	call_expr_t *data;

	data = slab_alloc(sizeof(*data));

	node = expr_add_child(parent);
	node->type = E_CALL;
	node->data = data;

	if (parse_inline_call(settings, parent, mod, data, tokens, tok)
	    == ERR_WAS_BUILTIN) {
		memset(node, 0, sizeof(*node));
		node->type = E_SKIP;
	}
}

/**
 * ident.ident([arg, ...])
 */
void parse_member_call(settings_t *settings, expr_t *parent, expr_t *mod,
		       token_list *tokens, token *tok)
{
	expr_t *node;
	call_expr_t *data;

	data = slab_alloc(sizeof(*data));

	node = expr_add_child(parent);
	node->type = E_CALL;
	node->data = data;

	if (tok->type != T_IDENT) {
		error_at(tokens->source, tok->value, tok->len,
			 "expected object name");
	}

	data->object_name = slab_strndup(tok->value, tok->len);

	tok = next_tok(tokens); /* . */
	tok = next_tok(tokens); /* method ident */

	if (parse_inline_call(settings, parent, mod, data, tokens, tok)
	    == ERR_WAS_BUILTIN) {
		memset(node, 0, sizeof(*node));
		node->type = E_SKIP;
	}
}
