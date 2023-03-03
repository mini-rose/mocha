/* nxg/error.c - print error and warning messages
   Copyright (c) 2023 mini-rose */

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
	fputs("\e[33mwarning\e[0m: ", stderr);
	vfprintf(stderr, format, ap);
	fputc('\n', stderr);
	va_end(ap);
}

noreturn void error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	fputs("\e[31merror\e[0m: ", stderr);
	vfprintf(stderr, format, ap);
	fputs("\n", stderr);
	va_end(ap);
	exit(1);
}

static void indent(int spaces)
{
	for (int i = 0; i < spaces; i++)
		fputc(' ', stderr);
}

typedef struct
{
	const char *title;
	const char *title_color;
	const char *highlight_color;
	const char *message_color;
} err_settings_t;

static void error_at_impl(file_t *source, err_settings_t *settings,
			  const char *pos, int len, const char *fix,
			  const char *format, va_list ap)
{
	const char *start = pos;
	const char *end = pos;
	const char *ptr = pos;
	const char *content = source->content;
	char line_str[11];
	int line = 1;

	for (const char *p = content; p < pos; p++)
		if (*p == '\n')
			line++;

	while (content < ptr && *ptr != '\n') {
		ptr--;
	}

	while (content < start && start[-1] != '\n')
		start--;

	while (*end && *end != '\n')
		end++;

	snprintf(line_str, 11, "%d", line);
	fprintf(stderr, "%s%s\e[0m in \e[1;98m%s\e[0m:\n\n",
		settings->title_color, settings->title, source->path);

	if (fix) {
		fprintf(stderr, "\t\e[96m");
		indent(pos - start - 1);
		for (int i = 0; i < len; i++)
			fputs("⌄", stderr);
		fprintf(stderr, " %s\e[0m\n", fix);
	}

	fprintf(stderr, "%s\t", line_str);

	ptr = start;
	while (ptr != end) {
		if (*ptr == '\t') {
			ptr++;
			continue;
		}

		fputc(*ptr, stderr);
		ptr++;
	}

	fprintf(stderr, "\n\t");
	indent(pos - start - strlen(line_str));
	fprintf(stderr, "%s", settings->title_color);

	for (int i = 0; i < len; i++)
		fputs("⌃", stderr);

	fprintf(stderr, " %s", settings->message_color);
	vfprintf(stderr, format, ap);
	fputs("\e[0m\n\n", stderr);

	va_end(ap);
}

noreturn void error_at(file_t *source, const char *pos, int len,
		       const char *format, ...)
{
	err_settings_t settings = {.title = "error",
				   .title_color = "\e[91m",
				   .highlight_color = "\e[1;91m",
				   .message_color = "\e[1;91m"};
	va_list ap;
	va_start(ap, format);

	error_at_impl(source, &settings, pos, len, NULL, format, ap);
	exit(1);
}

noreturn void error_at_with_fix(file_t *source, const char *pos, int len,
				const char *fix, const char *format, ...)
{
	err_settings_t settings = {.title = "error",
				   .title_color = "\e[91m",
				   .highlight_color = "\e[1;91m",
				   .message_color = "\e[1;91m"};
	va_list ap;
	va_start(ap, format);
	error_at_impl(source, &settings, pos, len, fix, format, ap);
	exit(1);
}

void warning_at(file_t *source, const char *pos, int len, const char *format,
		...)
{
	err_settings_t settings = {.title = "warning",
				   .title_color = "\e[33m",
				   .highlight_color = "\e[1;36m",
				   .message_color = "\e[1;33m"};
	va_list ap;
	va_start(ap, format);
	error_at_impl(source, &settings, pos, len, NULL, format, ap);
}

void warning_at_with_fix(file_t *source, const char *pos, int len,
			 const char *fix, const char *format, ...)
{
	err_settings_t settings = {.title = "warning",
				   .title_color = "\e[35m",
				   .highlight_color = "\e[1;36m",
				   .message_color = "\e[1;36m"};
	va_list ap;
	va_start(ap, format);
	error_at_impl(source, &settings, pos, len, fix, format, ap);
}
