/* std.builtin.stack - tools for tracking the callstack
   Copyright (c) 2023-2024 mini-rose */

#include "../x.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define X_STACKLIMIT 4096

struct stackentry
{
	char *symbol;
	char *file;
};

struct callstack
{
	struct stackentry **entries;
	int n;
};

static struct callstack x_callstack = {0};

void __x_stackpush(const char *symbol, const char *file)
{
	if (x_callstack.n >= X_STACKLIMIT) {
		printf(
		    "\e[1;91mstacktrace error\e[0m: reached call stack limit "
		    "of %d\n",
		    X_STACKLIMIT);
		exit(1);
	}

	x_callstack.entries =
	    realloc(x_callstack.entries,
		    sizeof(struct stackentry *) * (x_callstack.n + 1));
	x_callstack.entries[x_callstack.n] =
	    calloc(1, sizeof(struct stackentry));
	x_callstack.entries[x_callstack.n]->symbol = strdup(symbol);
	x_callstack.entries[x_callstack.n++]->file = strdup(file);
}

void __x_stackpop()
{
	struct stackentry *entry;

	x_callstack.entries =
	    realloc(x_callstack.entries,
		    sizeof(struct stackentry *) * (x_callstack.n--));
	entry = x_callstack.entries[x_callstack.n];

	free(entry->file);
	free(entry->symbol);
	free(entry);

	if (x_callstack.n == 0) {
		free(x_callstack.entries);
		x_callstack.n = 0;
		x_callstack.entries = NULL;
	}
}

void __x_stackdump()
{
	for (int i = x_callstack.n - 1; i >= 0; i--)
		printf("  %d: %s (%s)\n", x_callstack.n - i - 1,
		       x_callstack.entries[i]->symbol,
		       x_callstack.entries[i]->file);
}
