/* parser/call.c - parse calls
   Copyright (c) 2023 mini-rose */

#include "../alloc.h"
#include "../module.h"
#include "../parser.h"
#include "../tokenize.h"
#include "../type.h"
#include "../utils/error.h"
#include "../utils/utils.h"

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

bool is_builtin_decl(token *name)
{
	static const char *builtins[] = {"__builtin_decl",
					 "__builtin_decl_mangled"};

	for (size_t i = 0; i < LEN(builtins); i++) {
		if (!strncmp(builtins[i], name->value, name->len))
			return true;
	}

	return false;
}

bool is_builtin_value(token *name)
{
	static const char *builtins[] = {"__builtin_sizeof"};

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

err_t parse_builtin_decl(expr_t *parent, expr_t *mod, token_list *tokens,
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

err_t parse_builtin_value(expr_t *mod, value_expr_t *result, token_list *tokens,
			  token *tok)
{
	int type_size;

	/* Parse the builtin function that returns a value. */

	if (!strncmp(tok->value, "__builtin_sizeof", tok->len)) {
		tok = next_tok(tokens);
		tok = next_tok(tokens);

		if (!is_type(tokens, tok)) {
			error_at(tokens->source, tok->value, tok->len,
				 "sizeof requires a type");
		}

		type_size = type_sizeof(parse_type(mod, tokens, tok));
		result->type = VE_LIT;
		result->literal = slab_alloc(sizeof(literal_expr_t));
		result->literal->type = type_new_plain(PT_I64);
		result->literal->v_int = type_size;
		result->return_type = result->literal->type;

		tok = next_tok(tokens);
		if (tok->type != T_RPAREN) {
			error_at_with_fix(
			    tokens->source, tok->value, tok->len, ")",
			    "sizeof only takes a single argument");
		}
	}

	return ERR_OK;
}

static err_t parse_inline_call_impl(settings_t *settings, expr_t *parent,
				    expr_t *mod, expr_t *node,
				    value_expr_t *value, char *object_name,
				    token_list *tokens, token *tok)
{
	arg_tokens_t arg_tokens = {0};
	token *fn_name_tok;
	value_expr_t *self_arg = NULL;
	bool static_method = false;
	value_expr_t *arg;
	call_expr_t *data;

	if (is_builtin_decl(tok))
		return parse_builtin_decl(parent, mod, tokens, tok);

	if (node) {
		node->type = E_CALL;
		node->data = slab_alloc(sizeof(call_expr_t));
		data = node->data;
	}

	if (value) {
		value->type = VE_CALL;
		value->call = slab_alloc(sizeof(call_expr_t));
		data = value->call;
	}

	if (is_builtin_value(tok)) {
		/* If we're not in value-mode, we cannot expect any builtin
		   functions that resolve to values, like __builtin_sizeof. */
		if (node) {
			error_at(tokens->source, tok->value, tok->len,
				 "this builtin returns a value, which cannot "
				 "be used here");
		}

		return parse_builtin_value(mod, value, tokens, tok);
	}

	data->name = slab_strndup(tok->value, tok->len);
	data->object_name = object_name;
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
			error_at(tokens->source, tok->value - 1, 1,
				 "missing `,`");
		}

		tok = next_tok(tokens);
	}

	fn_candidates_t *resolved;

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
				error_at(tokens->source, o_name->value,
					 o_name->len,
					 "not found in this scope");
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
				 fn_name_tok->len, "method not found in `%s`",
				 obj_type->v_object->name);
		}

	} else {
		resolved = module_find_fn_candidates(mod, data->name);
	}

	/* Bare call */
	if (!resolved->n_candidates) {
		error_at(tokens->source, fn_name_tok->value, fn_name_tok->len,
			 "cannot find function in this scope");
	}

	int type_matching_tries;
	bool try_next;

	type_matching_tries = 0;

try_matching_types_again:
	type_matching_tries++;

	if (type_matching_tries > 16) {
		error_at(tokens->source, fn_name_tok->value, fn_name_tok->len,
			 "trying to convert arguments, failed 16 times");
	}

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

		/* Check if we can convert different types. */

		for (int j = 0; j < match->n_params; j++) {
			if (type_cmp(data->args[j]->return_type,
				     match->params[j]->type)) {
				continue;
			}

			/* If we only have one candidate, treat the failure to
			   cast as an error. */

			bool error_on_cast_failure =
			    resolved->n_candidates ? true : false;

			if (type_can_cast(data->args[j]->return_type,
					  match->params[j]->type)
			    && (data->args[j] = value_cast(
				    data->args[j], match->params[j]->type,
				    error_on_cast_failure, tokens,
				    arg_tokens.tokens[j]))) {
				goto try_matching_types_again;
			}
		}

		if (try_next)
			continue;
	}

	if (resolved->n_candidates == 1) {
		fprintf(stderr,
			"\033[36minfo:\033[0m found one candidate which "
			"does not match:\n");
	} else {
		fprintf(stderr,
			"\033[36minfo:\033[0m found "
			"\033[34m%d\033[0m potential "
			"candidates:\n",
			resolved->n_candidates);
	}

	for (int i = 0; i < resolved->n_candidates; i++) {
		char *sig = fn_str_signature(resolved->candidate[i], true);
		fprintf(stderr, "%*s• %s\n", (int) strlen("info: "), " ", sig);
	}

	fputc('\n', stdout);

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

	bool all_static = true;
	for (int i = 0; i < resolved->n_candidates; i++) {
		if (!resolved->candidate[i]->is_static) {
			all_static = false;
			break;
		}
	}

	char fix[1024];
	token *obj_name = fn_name_tok;

	if (all_static && resolved->n_candidates) {
		snprintf(fix, 1024, "%s.%s",
			 resolved->candidate[0]->object->name, data->name);

		obj_name = before_tok(tokens, before_tok(tokens, obj_name));
		error_at_with_fix(
		    tokens->source, obj_name->value,
		    fn_name_tok->value - obj_name->value + fn_name_tok->len,
		    fix, "cannot call static method from object instance");
	} else {
		error_at(tokens->source, fn_name_tok->value, fn_name_tok->len,
			 "could not find a matching overload");
	}

end:
	/* Common case: print() takes both a string reference and a string copy
	   to support literals. This also allows the user to pass copies when
	   a reference would be the better option. [prefer-ref] */

	if (!strcmp(data->name, "print") && settings->warn_prefer_ref) {
		if (data->n_args == 0)
			goto skip_ref_warn;

		if (!type_is_string(data->args[0]->return_type))
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

	if (value)
		value->return_type = type_copy(data->func->return_type);

skip_ref_warn:
	return ERR_OK;
}

err_t parse_inline_call_value(settings_t *settings, expr_t *parent, expr_t *mod,
			      value_expr_t *value, char *object_name,
			      token_list *tokens, token *tok)
{
	return parse_inline_call_impl(settings, parent, mod, NULL, value,
				      object_name, tokens, tok);
}

err_t parse_inline_call_node(settings_t *settings, expr_t *parent, expr_t *mod,
			     expr_t *node, char *object_name,
			     token_list *tokens, token *tok)
{
	return parse_inline_call_impl(settings, parent, mod, node, NULL,
				      object_name, tokens, tok);
}

/**
 * ident([arg, ...])
 */
void parse_call(settings_t *settings, expr_t *parent, expr_t *mod,
		token_list *tokens, token *tok)
{
	expr_t *node;

	node = expr_add_child(parent);
	parse_inline_call_node(settings, parent, mod, node, NULL, tokens, tok);
}

/**
 * ident.ident([arg, ...])
 */
void parse_member_call(settings_t *settings, expr_t *parent, expr_t *mod,
		       token_list *tokens, token *tok)
{
	expr_t *node;
	token *object_name_tok;
	char *object_name;

	node = expr_add_child(parent);

	if (tok->type != T_IDENT) {
		error_at(tokens->source, tok->value, tok->len,
			 "expected object name");
	}

	object_name_tok = tok;
	tok = next_tok(tokens); /* . */
	tok = next_tok(tokens); /* method ident */

	object_name =
	    slab_strndup(object_name_tok->value, object_name_tok->len);
	parse_inline_call_node(settings, parent, mod, node, object_name, tokens,
			       tok);

	if (node->type == E_CALL) {
		E_AS_CALL(node->data)->object_name = object_name;
	}
}
