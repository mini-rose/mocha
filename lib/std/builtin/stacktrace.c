/* std.builtin.stacktrace - tools for tracking the callstack
   Copyright (c) 2023 mini-rose */

#include "cf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static struct callstack cf_callstack = {0};

void __cf_stackpush(const char *symbol, const char *file)
{
	if (cf_callstack.n >= CF_STACKLIMIT) {
		printf(
		    "\e[1;91mstacktrace error\e[0m: reached call stack limit "
		    "of %d\n",
		    CF_STACKLIMIT);
		exit(1);
	}

	cf_callstack.entries =
	    realloc(cf_callstack.entries,
		    sizeof(struct stackentry *) * (cf_callstack.n + 1));
	cf_callstack.entries[cf_callstack.n] =
	    calloc(1, sizeof(struct stackentry));
	cf_callstack.entries[cf_callstack.n]->symbol = strdup(symbol);
	cf_callstack.entries[cf_callstack.n++]->file = strdup(file);
}

void __cf_stackpop()
{
	struct stackentry *entry;

	cf_callstack.entries =
	    realloc(cf_callstack.entries,
		    sizeof(struct stackentry *) * (cf_callstack.n--));
	entry = cf_callstack.entries[cf_callstack.n];

	free(entry->file);
	free(entry->symbol);
	free(entry);

	if (cf_callstack.n == 0) {
		free(cf_callstack.entries);
		cf_callstack.n = 0;
		cf_callstack.entries = NULL;
	}
}

void __cf_stackdump()
{
	for (int i = cf_callstack.n - 1; i >= 0; i--)
		printf("  %d: %s (%s)\n", cf_callstack.n - i - 1,
		       cf_callstack.entries[i]->symbol,
		       cf_callstack.entries[i]->file);
}
