#include "nxg/tokenize.h"

#include <nxg/error.h>
#include <nxg/parser.h>
#include <stdbool.h>
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

	for (int i = 0; i < function->n_args; i++) {
		free(function->args[i]->name);
		free(function->args[i]);
	}

	free(function->args);
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

static plain_type type_index(const char *str, int length)
{
	static const char *types[] = {"",    "str", "i8",  "i16",  "i32",
				      "i64", "f32", "f64", "bool", ""};

	for (int i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		if (!strncmp(str, types[i], length))
			return i;

	return 0;
}

static void fn_args_append(fn_expr_t *fn, token *name, token *type)
{
	fn->args = realloc(fn->args, sizeof(arg_t) * ++fn->n_args);
	fn->args[fn->n_args - 1] = malloc(sizeof(arg_t));
	fn->args[fn->n_args - 1]->name = strndup(name->value, name->len);
	fn->args[fn->n_args - 1]->type = type_index(type->value, type->len);
}

#define TOK_IS(TOK, TYPE, VALUE)                                               \
 (((TOK)->type == (TYPE)) && !strncmp((TOK)->value, VALUE, (TOK)->len))

static int index_of_tok(token_list *tokens, token *tok)
{
	for (int i = 0; i < tokens->length; i++) {
		if (tokens->tokens[i] == tok) {
			return i;
		}
	}

	return 0;
}

// Check pattern of variable declaration
static bool is_var_decl(token_list *list, token *cur)
{
	int index = index_of_tok(list, cur);
	token *punct;
	token *type;

	if (list->tokens[index + 1]->type == T_END
	 && list->tokens[index + 2]->type == T_END)
		return false;

	punct = list->tokens[index + 1];
	type = list->tokens[index + 2];

	return punct->type == T_PUNCT && !strncmp(punct->value, ":", punct->len)
	    && type->type == T_DATATYPE;
}

// Check pattern of variable definition
static bool is_var_def(token_list *list, token *cur)
{
	int index = index_of_tok(list, cur);
	token *punct;
	token *type;
	token *operator;

	punct = list->tokens[index + 1];
	type = list->tokens[index + 2];
	operator= list->tokens[index + 3];

	return punct->type == T_PUNCT && !strncmp(punct->value, ":", punct->len)
	    && type->type == T_DATATYPE && operator->type == T_OPERATOR;
}

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
	data->n_args = 0;

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
	int is_first = 1;
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		token *name;
		token *type;

		if (TOK_IS(tok, T_END, "")) {
			error_at(tokens->source->content, tok->value,
				 "expected ')'.");

			return ERR_SYNTAX;
		}

		if ((TOK_IS(tok, T_PUNCT, ",") && !is_first)
		    || tok->type == T_NEWLINE) {
			tok = next_tok(NULL);
			continue;
		}

		if (tok->type != T_IDENT) {
			error_at(tokens->source->content, tok->value,
				 "expected argument name");
			return ERR_SYNTAX;
		} else {
			name = tok;

			tok = next_tok(NULL);
			if (!TOK_IS(tok, T_PUNCT, ":")) {
				error_at(tokens->source->content, tok->value,
					 "missing type for '%.*s' argument.",
					 name->len, name->value);
				return ERR_SYNTAX;
			}

			tok = next_tok(NULL);
			if (tok->type != T_DATATYPE) {
				error_at(tokens->source->content, tok->value,
					 "expected type got: '%.*s'.", tok->len,
					 tok->value);
				return ERR_SYNTAX;
			}

			type = tok;
		}

		fn_args_append(data, name, type);

		is_first = 0;
		tok = next_tok(NULL);
	}

	data->return_type = T_VOID;

	/* return type (optional) */
	tok = next_tok(NULL);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = next_tok(NULL);
		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "missing function return type");
			return ERR_SYNTAX;
		}

		data->return_type = type_index(tok->value, tok->len);
	}

	/* opening & closing braces */
	if (TOK_IS(tok, T_NEWLINE, ""))
		tok = next_tok(NULL);

	if (!TOK_IS(tok, T_PUNCT, "{")) {
		error_at(tokens->source->content, tok->value,
			 "missing opening brace for `%s` function",
			 data->name);
		return ERR_SYNTAX;
	}

	tok = next_tok(NULL);
	while (!TOK_IS(tok, T_PUNCT, "}")) {
		if (tok->type == T_NEWLINE) {
			tok = next_tok(NULL);
			continue;
		}

		tok = next_tok(NULL);
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
