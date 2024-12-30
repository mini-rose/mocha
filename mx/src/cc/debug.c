/* debug.c
   Copyright (c) 2024 mini-rose */

#include <stdarg.h>
#include <stdio.h>

void debug_impl(const char *func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	fprintf(stderr, "%% \033[90m%-24s | \033[m", func);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");

	va_end(args);
}
