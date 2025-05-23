/* error.h - error & warning messages
   Copyright (c) 2023 mini-rose */

#pragma once

#include <mx/utils/file.h>
#include <stdnoreturn.h>

typedef enum
{
	ERR_OK,
	ERR_SYNTAX,
	ERR_WAS_BUILTIN,
	ERR_WAS_VALUE
} err_t;

void warning(const char *format, ...);
noreturn void error(const char *format, ...);
noreturn void error_at(file_t *source, const char *pos, int len,
		       const char *format, ...);
noreturn void error_at_with_fix(file_t *source, const char *pos, int len,
				const char *fix, const char *format, ...);
void warning_at(file_t *source, const char *pos, int len, const char *format,
		...);
void warning_at_with_fix(file_t *source, const char *pos, int len,
			 const char *fix, const char *format, ...);
