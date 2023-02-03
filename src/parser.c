#include "nxg/tokenize.h"
#include "nxg/type.h"

#include <nxg/error.h>
#include <nxg/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static node *node_new()
{
	return (node *) calloc(1, sizeof(node));
}

static void node_destroy(node *n)
{
	node *next;

	if (!n->next)
		goto free;

	while (n->next) {
		next = n->next;
		n->free(n->data);
		free(n);
		n = next;
	}

free:
	n->free(n->data);
	free(n);
}

static module_t *module_new()
{
	return (module_t *) calloc(1, sizeof(module_t));
}

static void module_destroy(module_t *m)
{
	if (m->name)
		free(m->name);

	free(m);
}

static token *next_tok(const token_list *list)
{
	static const token_list *self = NULL;
	static int current = -1;

	return (self == NULL)
		? (self = list)->tokens[0]
		: self->tokens[++current];
}

#define TOK_IS(TOK, TYPE, STR)                                                 \
 ((TOK)->type == TYPE && !strncmp((TOK)->value, STR, (TOK)->len))

static arg_t *arg_new()
{
	return (arg_t *) calloc(1, sizeof(arg_t));
}

static void arg_destroy(arg_t *a)
{
	if (a->name)
		free(a->name);

	free(a);
}

static func_t *func_new()
{
	return (func_t *) calloc(1, sizeof(func_t));
}

static void func_destroy(func_t *func)
{
	int i;

	if (func->name)
		free(func->name);

	for (i = 0; i < func->n_args; i++)
		arg_destroy(func->args[i]);

	if (func->args)
		free(func->args);

	free(func);
}

static void func_add_arg(func_t *func, token *name, token *type)
{
	func->args = realloc(func->args, ++func->n_args * sizeof(arg_t));
	func->args[func->n_args - 1] = arg_new();
	char *type_str = strndup(type->value, type->len);
	func->args[func->n_args - 1]->type = get_type(type_str);
	free(type_str);
	func->args[func->n_args - 1]->name = strndup(name->value, name->len);
}

// Returns next node
static node *node_add_next(node *prev)
{
	node *next = node_new();

	while (prev->next)
		prev = prev->next;

	prev->next = next;
	next->prev = prev;

	return next;
}

static void fn_print(func_t *func)
{
	int i;

	printf("NAME: %s\n", func->name);
	printf("RETURN: %i\n", func->return_type);
	fputs("ARGS: \n\t", stdout);

	for (i = 0; i < func->n_args; i++) {
		fputs(func->args[i]->name, stdout);
		printf(": %i\n\t", func->args[i]->type);
	}

	fputs("\n", stdout);
}

static token *parse_fn(token_list *tokens, node *n)
{
	func_t *data;
	node *node;
	token *tok;

	data = func_new();

	node = node_add_next(n);
	node->type = N_FUNCTION;
	node->data = data;
	node->free = (node_free) func_destroy;

	tok = next_tok(NULL);

	if (tok->type != T_IDENT) {
		error_at(tokens->source->content, tok->value,
			 "expected function name, got %s", tokname(tok->type));
		exit(1);
	}

	data->name = strndup(tok->value, tok->len);

	tok = next_tok(NULL);
	if (!TOK_IS(tok, T_PUNCT, "(")) {
		error_at(tokens->source->content, tok->value,
			 "expected `(` after function name");
		exit(1);
	}

	tok = next_tok(NULL);
	while (!TOK_IS(tok, T_PUNCT, ")")) {
		static int is_first = 1;
		token *arg_name;
		token *arg_type;

		if (TOK_IS(tok, T_END, "")) {
			error_at(tokens->source->content, tok->value,
				 "expected ')'.");
			exit(1);
		}

		if ((TOK_IS(tok, T_PUNCT, ",") && !is_first)
		    || tok->type == T_NEWLINE) {
			tok = next_tok(NULL);
			continue;
		}

		if (tok->type != T_IDENT) {
			error_at(tokens->source->content, tok->value,
				 "expected argument name");
			exit(1);
		}

		arg_name = tok;

		tok = next_tok(NULL);
		if (!TOK_IS(tok, T_PUNCT, ":")) {
			error_at(tokens->source->content, tok->value,
				 "missing type for '%.*s' argument.",
				 arg_name->len, arg_name->value);
			exit(1);
		}

		tok = next_tok(NULL);
		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "expected type got: '%.*s'.", tok->len,
				 tok->value);
			exit(1);
		}

		arg_type = tok;

		func_add_arg(data, arg_name, arg_type);

		is_first = 0;
		tok = next_tok(NULL);
	}

	data->return_type = T_VOID;

	tok = next_tok(NULL);
	if (TOK_IS(tok, T_PUNCT, ":")) {
		tok = next_tok(NULL);
		if (tok->type != T_DATATYPE) {
			error_at(tokens->source->content, tok->value,
				 "missing function return type");
			exit(1);
		}

		char *ret_name = strndup(tok->value, tok->len);
		data->return_type = get_type(ret_name);
		free(ret_name);
	}

	if (TOK_IS(tok, T_NEWLINE, ""))
		tok = next_tok(NULL);

	if (!TOK_IS(tok, T_PUNCT, "{")) {
		error_at(tokens->source->content, tok->value,
			 "missing opening brace for `%s` function", data->name);
		exit(1);
	}

	tok = next_tok(NULL);
	while (!TOK_IS(tok, T_PUNCT, "}")) {
		if (tok->type == T_NEWLINE)
			goto next;

next:
		tok = next_tok(NULL);
	}

	fn_print(data);

	return tok;
}

void parse(token_list *tokens)
{
	node *node = node_new();
	module_t *mod = module_new();
	token *tok = next_tok(tokens);

	node->type = N_MODULE;
	node->free = (node_free) module_destroy;
	node->data = mod;

	mod->name = strdup("__main__");

	// Parse function in global scope
	while ((tok = next_tok(NULL)) && tok->type != T_END) {
		if (tok->type == T_NEWLINE)
			continue;

		if (TOK_IS(tok, T_KEYWORD, "fn")) {
			tok = parse_fn(tokens, node);
		} else  {
			error_at(tokens->source->content, tok->value,
				 "only functions are allowed at the top-level");
			exit(1);
		}
	}

	node_destroy(node);
}
