#include <nxg/utils/error.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

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
	const char *ptr = pos;
	int line = 1;
	int tabs = 0;

	for (const char *p = content; p < pos; p++)
		if (*p == '\n')
			line++;

	while (content < ptr && *ptr != '\n') {
		if (*ptr == '\t')
			tabs++;
		ptr--;
	}

	while (content < start && start[-1] != '\n')
		start--;

	while (*end && *end != '\n')
		end++;

	fprintf(stderr, "%i\t%.*s\n\t", line, (int) (end - start), start);

	for (int i = 0; i < tabs; i++)
		fputc('\t', stderr);
	for (int i = 0; i < (pos - start - tabs); i++)
		fputc(' ', stderr);

	fprintf(stderr, "\e[33m^ \e[31m");
	vfprintf(stderr, format, ap);
	fputs("\e[0m\n", stderr);

	va_end(ap);
	exit(1);
}
