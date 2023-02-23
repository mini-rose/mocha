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

bool is_builtin_function(token *name)
{
	static const char *builtins[] = {"__builtin_decl",
					 "__builtin_decl_mangled"};

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

err_t parse_inline_call(expr_t *parent, expr_t *mod, call_expr_t *data,
			token_list *tokens, token *tok)
{
	arg_tokens_t arg_tokens = {0};
	token *fn_name_tok;
	value_expr_t *arg;

	if (is_builtin_function(tok))
		return parse_builtin_call(parent, mod, tokens, tok);

	data->name = slab_strndup(tok->value, tok->len);
	fn_name_tok = tok;

	tok = next_tok(tokens);
	tok = next_tok(tokens);

	/* Arguments - currently only support variable names & literals
	 */
	while (tok->type != T_RPAREN) {
		if (is_call(tokens, tok)) {
			arg = call_add_arg(data);
			add_arg_token(&arg_tokens, tok);

			arg->type = VE_CALL;
			arg->call = slab_alloc(sizeof(*arg->call));
			parse_inline_call(parent, mod, arg->call, tokens, tok);

			arg->return_type =
			    type_copy(arg->call->func->return_type);

		} else if (is_single_value(tokens, tok)) {
			arg = slab_alloc(sizeof(*arg));
			add_arg_token(&arg_tokens, tok);
			arg = parse_value_expr(parent, mod, arg, tokens, tok);
			call_push_arg(data, arg);

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

		if (tok->type == T_RPAREN)
			break;

		if (tok->type != T_COMMA) {
			error_at_with_fix(
			    tokens->source, tok->value, tok->len, ",",
			    "expected comma seperating the arguments");
		}

		tok = next_tok(tokens);
	}

	fn_candidates_t *resolved = module_find_fn_candidates(mod, data->name);

	if (!resolved->n_candidates) {
		/* Check if maybe the user missed an import */
		char *fix = NULL;

		if (!strcmp(data->name, "print") || !strcmp(data->name, "write")
		    || !strcmp(data->name, "close")
		    || !strcmp(data->name, "open"))
			fix = ", did you mean to `use std.io`?";

		if (!strcmp(data->name, "system")
		    || !strcmp(data->name, "sleep")
		    || !strcmp(data->name, "getenv")
		    || !strcmp(data->name, "version"))
			fix = ", did you mean to `use std.sys`?";

		if (!strcmp(data->name, "write"))
			fix = ", did you mean to `use std.io`?";

		error_at(tokens->source, fn_name_tok->value, fn_name_tok->len,
			 "no function named `%s` found%s", data->name,
			 fix ? fix : "");
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

		goto end;
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
	return ERR_OK;
}

/**
 * ident([arg, ...])
 */
void parse_call(expr_t *parent, expr_t *mod, token_list *tokens, token *tok)
{
	expr_t *node;
	call_expr_t *data;

	data = slab_alloc(sizeof(*data));

	node = expr_add_child(parent);
	node->type = E_CALL;
	node->data = data;

	if (parse_inline_call(parent, mod, data, tokens, tok)
	    == ERR_WAS_BUILTIN) {
		memset(node, 0, sizeof(*node));
		node->type = E_SKIP;
	}
}
