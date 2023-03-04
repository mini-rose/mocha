#include <mocha/utils/error.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

noreturn void error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	fputs("\033[31merror\033[0m: ", stderr);
	vfprintf(stderr, format, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void warning(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	fputs("\033[33mwarning\033[0m: ", stderr);
	vfprintf(stderr, format, ap);
	fputc('\n', stderr);
	va_end(ap);
}
