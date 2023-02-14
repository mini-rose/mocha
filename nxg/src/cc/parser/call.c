#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static value_expr_t *call_add_arg(call_expr_t *call)
{
	value_expr_t *node = calloc(1, sizeof(*node));
	call->args =
	    realloc(call->args, sizeof(value_expr_t *) * (call->n_args + 1));
	call->args[call->n_args++] = node;
	return node;
}

bool is_builtin_function(token *name)
{
	static const char *builtins[] = {"__builtin_decl",
					 "__builtin_decl_mangled"};
	static const int n = sizeof(builtins) / sizeof(*builtins);

	for (int i = 0; i < n; i++) {
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

		decl->name = strndup(arg->value, arg->len);

		arg = next_tok(tokens);
		if (!TOK_IS(arg, T_PUNCT, ",")) {
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

err_t parse_inline_call(expr_t *parent, expr_t *mod, call_expr_t *data,
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
		if (is_call(tokens, tok)) {
			arg = call_add_arg(data);
			add_arg_token(&arg_tokens, tok);

			arg->type = VE_CALL;
			arg->call = calloc(1, sizeof(*arg->call));
			parse_inline_call(parent, mod, arg->call, tokens, tok);

			arg->return_type =
			    type_copy(arg->call->func->return_type);

		} else if (tok->type == T_IDENT || TOK_IS(tok, T_PUNCT, "&")
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
			if (match->params[j]->type->kind != TY_POINTER)
				continue;

			if (data->args[j]->type != VE_REF) {
				error_at(
				    tokens->source, arg_tokens.tokens[j]->value,
				    arg_tokens.tokens[j]->len,
				    "%s takes a reference to `%s` here",
				    match->name,
				    type_name(match->params[j]->type->v_base));
				continue;
			}

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
void parse_call(expr_t *parent, expr_t *mod, token_list *tokens, token *tok)
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
