#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

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

static void indent(int tabs, int spaces)
{
	for (int i = 0; i < tabs; i++)
		fputc('\t', stderr);
	for (int i = 0; i < spaces; i++)
		fputc(' ', stderr);
}

noreturn static void error_at_impl(file_t *source, const char *pos, int len,
				   const char *fix, const char *format,
				   va_list ap)
{
	const char *start = pos;
	const char *end = pos;
	const char *ptr = pos;
	const char *content = source->content;
	int line = 0;
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

	fprintf(stderr, "\e[1;91merror\e[0m in \e[1;98m%s\e[0m:\n\n",
		source->path);

	if (fix) {
		fprintf(stderr, "\t\e[96m");
		indent(tabs, pos - start - tabs);
		fprintf(stderr, "%s\e[0m\n", fix);
		fprintf(stderr, "\t\e[96m");
		indent(tabs, pos - start - tabs);
		for (int i = 0; i < len; i++)
			fputc('v', stderr);
		fprintf(stderr, "\e[0m\n");
	}

	fprintf(stderr, "%d\t", line);

	ptr = start;
	while (ptr != end) {
		if (ptr == pos)
			fprintf(stderr, "\e[1;91m");
		if (ptr == (pos + len))
			fprintf(stderr, "\e[0m");
		fputc(*ptr, stderr);
		ptr++;
	}

	fprintf(stderr, "\n\t");
	indent(tabs, pos - start - tabs);
	fprintf(stderr, "\e[32m^");

	for (int i = 0; i < len - 1; i++)
		fputc('~', stderr);

	fprintf(stderr, " \e[31m");
	vfprintf(stderr, format, ap);
	fputs("\e[0m\n", stderr);

	va_end(ap);
	exit(1);
}

noreturn void error_at(file_t *source, const char *pos, int len,
		       const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	error_at_impl(source, pos, len, NULL, format, ap);
}

noreturn void error_at_with_fix(file_t *source, const char *pos, int len,
				const char *fix, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	error_at_impl(source, pos, len, fix, format, ap);
}
