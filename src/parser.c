#include <nxg/parser.h>
#include <stdio.h>
#include <stdlib.h>

static token *next(const token_list *list)
{
	static const token_list *self = NULL;
	static int current = -1;

	if (self == NULL)
		self = list;

	return self->tokens[current++];
}

expr *parse(token_list *list)
{
	token *current = next(list);

	while ((current = next(NULL)) && current->type != END) {
		// TODO: implement that
	}

	return NULL;
}
