#pragma once

#include <stdnoreturn.h>

typedef enum
{
	ERR_OK,
	ERR_SYNTAX,
	ERR_WAS_BUILTIN
} err_t;

void warning(const char *format, ...);
noreturn void error(const char *format, ...);
noreturn void error_at(const char *content, const char *pos, const char *format, ...);
