#include "nxg/tokenize.h"
#include <nxg/error.h>
#include <nxg/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void mod_expr_free(mod_expr_t *module)
{
	free(module->name);
}

static void fn_expr_free(fn_expr_t *function)
{
	free(function->name);
}

static void var_expr_free(var_expr_t *variable)
{
	free(variable->name);
	free(variable->value);
}

static void var_decl_expr_free(var_decl_expr_t *variable)
{
	free(variable->name);
}

static token *next_tok(const token_list *list)
{
	static const token_list *self = NULL;
	static int current = -1;

	return (self == NULL)
		? (self = list)->tokens[0]
		: self->tokens[++current];
}

static expr_t *expr_add_child(expr_t *parent)
{
	expr_t *node = calloc(sizeof(*node), 1);
	expr_t *walker = parent->child;

	if (!parent->child)
		return parent->child = node;

	/* find the first empty slot in the linked list */
	while (walker->next)
		walker = walker->next;

	walker->next = node;
	return node;
}

#define TOK_IS(TOK, TYPE, VALUE)                                               \
 (((TOK)->type == (TYPE)) && !strncmp((TOK)->value, VALUE, (TOK)->len))

static err_t parse_fn(token_list *tokens, expr_t *parent)
{
	fn_expr_t *data;
	expr_t *node;
	token *tok;

	node = expr_add_child(parent);
	data = calloc(sizeof(*data), 1);

	node->type = E_FUNCTION;
	node->data = data;
	node->data_free = (expr_free_handle) fn_expr_free;

	tok = next_tok(NULL);

	/* name */
	if (tok->type != T_IDENT) {
		error_at(tokens->source->content, tok->value,
			 "expected function name, got %s", tokname(tok->type));
		return ERR_SYNTAX;
	}

	data->name = strndup(tok->value, tok->len);

	/* parameters (currently skip) */
	tok = next_tok(NULL);
	if (!TOK_IS(tok, T_PUNCT, "(")) {
		error_at(tokens->source->content, tok->value,
			 "expected `(` after function name");
		return ERR_SYNTAX;
	}

	tok = next_tok(NULL);
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		if (tok->type == T_END) {
			error_at(tokens->source->content, tok->value,
				 "end of function parameter list for `%s()` "
				 "function not found",
				 data->name);
			return ERR_SYNTAX;
		}

		tok = next_tok(NULL);
	}

	/* return type (optional) */
	tok = next_tok(NULL);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = next_tok(NULL);
		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "missing function return type");
			return ERR_SYNTAX;
		}

		/* TODO: set data->return_type */
	}

	/* opening & closing braces */
	tok = next_tok(NULL);

	if (TOK_IS(tok, T_NEWLINE, ""))
		tok = next_tok(NULL);

	if (!TOK_IS(tok, T_PUNCT, "{")) {
		error_at(tokens->source->content, tok->value,
			 "missing opening brace for `%s` function",
			 data->name);
		return ERR_SYNTAX;
	}

	/* TODO: closing brace, all expr inside. */

	return 0;
}

static plain_type type_index(const char *str)
{
	static const char *types[] = {"",    "str", "i8",  "i16",  "i32",
				      "i64", "f32", "f64", "bool", ""};

	for (int i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		if (!strcmp(str, types[i]))
			return i;

	return 0;
}

static err_t parse_var(token_list *tokens, token *id, expr_t *parent)
{
	var_expr_t *data;
	expr_t *node;
	token *tok;

	node = expr_add_child(parent);
	data = calloc(sizeof(*data), 1);

	node->data = data;

	data->name = strndup(id->value, id->len);

	tok = next_tok(NULL);

	if (!TOK_IS(tok, T_PUNCT, ":")) {
		error_at(tokens->source->content, tok->value,
			 "missing type for '%s' variable.", data->name);
		return ERR_SYNTAX;
	} else {
		tok = next_tok(NULL);

		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "missing type name.");
			return ERR_SYNTAX;
		} else {

			char str[32];

			snprintf(str, sizeof(str), "%.*s", tok->len,
				 tok->value);

			plain_type type = type_index(str);

			if (type == 0) {
				error_at(tokens->source->content, tok->value,
					 "unknown type '%.*s' for variable",
					 tok->len, tok->value);
				return ERR_SYNTAX;
			}

			data->type = type;
		}
	}

	tok = next_tok(NULL);

	if (!TOK_IS(tok, T_OPERATOR, "=")) {
		node->type = E_VARDECL;
		node->data_free = (expr_free_handle) var_decl_expr_free;
	} else {
		node->type = E_VARDEF;
		node->data_free = (expr_free_handle) var_expr_free;

		tok = next_tok(NULL);

		if (!((tok->type == T_STRING || tok->type == T_NUMBER))) {
			error_at(tokens->source->content, tok->value,
				 "expected value.");
			return ERR_SYNTAX;
		}

		data->value = strndup(tok->value, tok->len);
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

	while ((current = next_tok(NULL)) && current->type != T_END) {
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
