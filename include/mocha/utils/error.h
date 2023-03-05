/* utils/error.h
   Copyright (c) 2023 mini-rose */

#pragma once

#include <mocha/utils/file.h>
#include <stdnoreturn.h>

noreturn void error(const char *format, ...);
noreturn void error_at(file_t *source, const char *pos, int len,
		       const char *format, ...);

void warning(const char *format, ...);
void warning_at(file_t *source, const char *pos, int len, const char *format,
		...);
