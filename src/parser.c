#include "nxg/error.h"

#include <nxg/parser.h>
#include <nxg/tokenize.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void mod_expr_free(mod_expr_t *module)
{
	free(module->name);
}

static token *next_tok(const token_list *list)
{
	static const token_list *self = NULL;
	static int current = -1;

	if (self == NULL)
		self = list;

	return self->tokens[current++];
}

static expr_t *expr_add_child(expr_t *parent)
{
	expr_t *node = calloc(sizeof(*node), 1);
	expr_t *walker = parent->child;

	if (!parent->child) {
		parent->child = node;
		return node;
	}

	/* find the first empty slot in the linked list */
	while (walker->next)
		walker = walker->next;

	walker->next = node;
	return node;
}

static void parse_fn(expr_t *parent) { }

expr_t *parse(file *source, token_list *list)
{
	token *current = next_tok(list);
	expr_t *module = calloc(sizeof(*module), 1);

	module->type = E_MODULE;
	module->data = malloc(sizeof(mod_expr_t));
	module->data_free = (expr_free_handle) mod_expr_free;
	E_AS_MOD(module->data)->name = strdup("__main__");

	token_list_print(list);

	while ((current = next_tok(NULL)) && current->type != T_END) {
		/* top-level: function decl */
		if (current->type == T_KEYWORD
		    && !strcmp(current->value, "fn")) {
			parse_fn(module);
		} else {
			error("invalid keyword");
		}
	}

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
