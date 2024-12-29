/* std.builtin.stack - tools for tracking the callstack
   Copyright (c) 2023-2024 mini-rose */

#include "../mocha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCHA_STACKLIMIT 4096

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

static struct callstack mocha_callstack = {0};

void __mocha_stackpush(const char *symbol, const char *file)
{
	if (mocha_callstack.n >= MOCHA_STACKLIMIT) {
		printf(
		    "\e[1;91mstacktrace error\e[0m: reached call stack limit "
		    "of %d\n",
		    MOCHA_STACKLIMIT);
		exit(1);
	}

	mocha_callstack.entries =
	    realloc(mocha_callstack.entries,
		    sizeof(struct stackentry *) * (mocha_callstack.n + 1));
	mocha_callstack.entries[mocha_callstack.n] =
	    calloc(1, sizeof(struct stackentry));
	mocha_callstack.entries[mocha_callstack.n]->symbol = strdup(symbol);
	mocha_callstack.entries[mocha_callstack.n++]->file = strdup(file);
}

void __mocha_stackpop()
{
	struct stackentry *entry;

	mocha_callstack.entries =
	    realloc(mocha_callstack.entries,
		    sizeof(struct stackentry *) * (mocha_callstack.n--));
	entry = mocha_callstack.entries[mocha_callstack.n];

	free(entry->file);
	free(entry->symbol);
	free(entry);

	if (mocha_callstack.n == 0) {
		free(mocha_callstack.entries);
		mocha_callstack.n = 0;
		mocha_callstack.entries = NULL;
	}
}

void __mocha_stackdump()
{
	for (int i = mocha_callstack.n - 1; i >= 0; i--)
		printf("  %d: %s (%s)\n", mocha_callstack.n - i - 1,
		       mocha_callstack.entries[i]->symbol,
		       mocha_callstack.entries[i]->file);
}
