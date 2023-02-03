#include <nxg/error.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void warning(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	fputs("\e[33mWARNING\e[0m: ", stderr);
	vfprintf(stderr, format, ap);
	fputc('\n', stderr);
	va_end(ap);
}

void error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	fputs("\e[31mERROR\e[0m: ", stderr);
	vfprintf(stderr, format, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(1);
}

void error_at(const char *content, const char *pos, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	const char *start = pos;
	const char *end = pos;
	int line = 1;

	for (const char *p = content; p < pos; p++)
		if (*p == '\n')
			line++;

	while (content < start && start[-1] != '\n')
		start--;

	while (*end && *end != '\n')
		end++;

	fprintf(stderr, "%i\t%.*s\n \t%*s↑ \e[31m", line, (int) (end - start),
		start, (int) (pos - start), "");

	vfprintf(stderr, format, ap);
	fputs("\e[0m\n", stderr);

	va_end(ap);
}
