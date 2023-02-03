#include <nxg/error.h>
#include <nxg/parser.h>
#include <nxg/tokenize.h>
#include <nxg/type.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TOK_IS(TOK, TYPE, VALUE)                                               \
 (((TOK)->type == (TYPE)) && !strncmp((TOK)->value, VALUE, (TOK)->len))

static void mod_expr_free(mod_expr_t *module)
{
	free(module->name);
}

static void fn_expr_free(fn_expr_t *function)
{
	free(function->name);

	for (int i = 0; i < function->n_params; i++) {
		free(function->params[i]->name);
		free(function->params[i]);
	}

	free(function->params);
}

static void var_expr_free(var_expr_t *variable)
{
	free(variable->name);
	free(variable->value);
}

static void ret_expr_free(var_expr_t *variable)
{
	free(variable->name);
	free(variable->value);
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

static expr_t *expr_add_next(expr_t *prev)
{
	expr_t *node = calloc(sizeof(*node), 1);

	while (prev->next)
		prev = prev->next;

	prev->next = node;
	return node;
}

static expr_t *expr_add_child(expr_t *parent)
{
	if (parent->child)
		return expr_add_next(parent);

	expr_t *node = calloc(sizeof(*node), 1);
	parent->child = node;
	return node;
}

static void fn_add_param(fn_expr_t *fn, token *name, plain_type type)
{
	fn->params = realloc(fn->params, sizeof(fn_param_t) * ++fn->n_params);
	fn->params[fn->n_params - 1] = malloc(sizeof(fn_param_t));
	fn->params[fn->n_params - 1]->name = strndup(name->value, name->len);
	fn->params[fn->n_params - 1]->type = type;
}

static bool is_func(token_list *list, const char *str)
{
	for (int i = 0; i < list->length; i++) {
		if (TOK_IS(list->tokens[i], T_KEYWORD, "fn")
		    && !strncmp(str, list->tokens[i + 1]->value,
				list->tokens[i + 1]->len)) {
			return true;
		}
	}

	return false;
}

static int index_of_tok(token_list *tokens, token *tok)
{
	for (int i = 0; i < tokens->length; i++)
		if (tokens->tokens[i] == tok)
			return i;

	return 0;
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
 * name: type = value
 */
static bool is_var_def(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter);
	if (!TOK_IS(tok, T_PUNCT, ":"))
		return false;

	tok = index_tok(tokens, tokens->iter + 1);
	if (tok->type != T_DATATYPE)
		return false;

	tok = index_tok(tokens, tokens->iter + 2);
	if (!TOK_IS(tok, T_OPERATOR, "="))
		return false;

	return true;
}

/**
 * name = value
 */
static bool is_var_assign(token_list *tokens, token *tok)
{
	if (tok->type != T_IDENT)
		return false;

	tok = index_tok(tokens, tokens->iter + 1);
	if (!TOK_IS(tok, T_PUNCT, "="))
		return false;

	return true;
}

static bool is_math_expr(token_list *list, token *start)
{
	int index = index_of_tok(list, start);

	for (int i = index; i < list->length; i++) {
		if (list->tokens[i]->type == T_NEWLINE)
			break;

		if (list->tokens[i]->type == T_OPERATOR)
			return true;
	}

	return false;
}

static err_t parse_var_decl(expr_t *parent, token_list *tokens, token *tok)
{
	var_decl_expr_t *data;
	expr_t *node;

	data = calloc(sizeof(*data), 1);
	data->name = strndup(tok->value, tok->len);

	tok = next_tok(tokens);
	data->type = plain_type_from(tok->value, tok->len);

	node = expr_add_child(parent);
	node->type = E_VARDECL;
	node->data = data;
	node->data_free = (expr_free_handle) var_decl_expr_free;

	return ERR_OK;
}

static err_t parse_var_def(expr_t *parent, token_list *tokens, token *current)
{
	var_expr_t *data;
	expr_t *node;
	token *tok;
	int index;
	char *value = NULL;

	node = expr_add_child(parent);
	data = calloc(sizeof(*data), 1);
	node->type = E_VARDEF;
	node->data = data;
	node->data_free = (expr_free_handle) var_expr_free;

	index = index_of_tok(tokens, current);

	data->name = strndup(current->value, current->len);
	data->with_vars = false;
	data->type = plain_type_from(tokens->tokens[index + 1]->value,
				     tokens->tokens[index + 1]->len);

	next_tok(tokens);
	next_tok(tokens);
	next_tok(tokens);

	while ((tok = next_tok(tokens))->type != T_NEWLINE) {
		if (tok->type == T_IDENT) {
			if (is_math_expr(tokens, tok)) {
				error_at(
				    tokens->source->content, tok->value,
				    "Math expressions are not supported yet");
				exit(1);
			}
		}
	}

	data->value = value;

	return ERR_OK;
}

static err_t parse_var_assign(expr_t *parent, token_list *tokens, token *tok)
{
	var_expr_t *data;

	return ERR_OK;
}

static err_t parse_fn(token_list *tokens, expr_t *module)
{
	fn_expr_t *data;
	expr_t *node;
	token *tok;

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

	data->name = strndup(tok->value, tok->len);

	/* parameters */
	tok = next_tok(tokens);
	if (!TOK_IS(tok, T_PUNCT, "(")) {
		error_at(tokens->source->content, tok->value,
			 "expected `(` after function name");
		return ERR_SYNTAX;
	}

	tok = next_tok(tokens);
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		plain_type type;
		token *name;

		if (tok->type == T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "the parameter name comes first - the type is "
				 "located after a colon like this: `%.*s %s`",
				 tok->len, tok->value,
				 plain_type_example_varname(
				     plain_type_from(tok->value, tok->len)));
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
				 "the `%.*s` parameter is missing a type, try "
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

		fn_add_param(data, name, type);

		if (TOK_IS(tok, T_PUNCT, ","))
			tok = next_tok(tokens);
	}

	/* return type (optional) */
	tok = next_tok(tokens);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = next_tok(tokens);
		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "expected return type, got `%.*s`", tok->len,
				 tok->value);
			return ERR_SYNTAX;
		}

		data->return_type = plain_type_from(tok->value, tok->len);
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

	tok = next_tok(tokens);
	int brace_level = 1;
	while (brace_level != 0) {
		if (tok->type == T_END) {
			if (brace_level == 1)
				error_at(tokens->source->content, tok->value,
					 "missing a closing brace");
			else
				error_at(tokens->source->content, tok->value,
					 "missing %d closing braces",
					 brace_level);
		}

		if (tok->type == T_NEWLINE) {
			tok = next_tok(tokens);
			continue;
		}

		if (TOK_IS(tok, T_PUNCT, "{"))
			brace_level++;
		if (TOK_IS(tok, T_PUNCT, "}"))
			brace_level--;

		/* statements inside the function */

		if (is_var_def(tokens, tok))
			parse_var_def(node, tokens, tok);
		else if (is_var_decl(tokens, tok))
			parse_var_decl(node, tokens, tok);
		else if (is_var_assign(tokens, tok))
			parse_var_assign(node, tokens, tok);
		else
			warning("unknown statement `%.*s`", tok->len,
				tok->value);

		tok = next_tok(tokens);
	}

	return 0;
}

expr_t *parse(token_list *tokens)
{
	token *current = next_tok(tokens);
	expr_t *module = calloc(sizeof(*module), 1);
	char *content = tokens->source->content;

	module->type = E_MODULE;
	module->data = malloc(sizeof(mod_expr_t));
	module->data_free = (expr_free_handle) mod_expr_free;
	E_AS_MOD(module->data)->name = strdup("__main__");

	token_list_print(tokens);

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
				 "only functions are allowed at the top-level");
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
	static const char *names[] = {"MODULE", "FUNCTION", "CALL",  "RETURN",
				      "VARDEF", "VARDECL",  "ASSIGN"};
	static const int n_names = sizeof(names) / sizeof(*names);

	if (type >= 0 && type < n_names)
		return names[type];
	return "<EXPR>";
}

static const char *func_str_signature(fn_expr_t *func)
{
	static char sig[1024];
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
	var_expr_t *var;

	switch (expr->type) {
	case E_MODULE:
		snprintf(info, 512, "%s", E_AS_MOD(expr->data)->name);
		break;
	case E_FUNCTION:
		snprintf(info, 512, "%s",
			 func_str_signature(E_AS_FN(expr->data)));
		break;
	case E_VARDECL:
		snprintf(info, 512, "%s %s",
			 plain_type_name(E_AS_VDECL(expr->data)->type),
			 E_AS_VDECL(expr->data)->name);
		break;
	case E_VARDEF:
		var = E_AS_VDEF(expr->data);
		snprintf(info, 512, "%s: %s = %s", var->name,
			 plain_type_name(var->type), var->value);
		break;
	default:
		info[0] = 0;
	}

	return info;
}

static void expr_print_level(expr_t *expr, int level)
{
	expr_t *walker;

	for (int i = 0; i < level; i++) {
		fputs("  ", stdout);
	}

	/* Info about this expression. */
	printf("%s %s\n", expr_typename(expr->type), expr_info(expr));

	walker = expr->next;
	while (walker && (walker = walker->next)) {
		expr_print_level(walker, level);
	}

	if (expr->child)
		expr_print_level(expr->child, level + 1);
}

void expr_print(expr_t *expr)
{
	expr_print_level(expr, 0);
}
