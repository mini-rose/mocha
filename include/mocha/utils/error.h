/* utils/error.h
   Copyright (c) 2023 mini-rose */

#pragma once

#ifndef _UTILS_H
# error                                                                         \
     "Never include <mocha/utils/error.h> directly; use <mocha/utils.h> instead"
#endif

#include <stdnoreturn.h>

noreturn void error(const char *format, ...);
noreturn void error_at(file_t *source, const char *pos, int len,
		       const char *format, ...);

void warning(const char *format, ...);
void warning_at(file_t *source, const char *pos, int len, const char *format,
		...);

void __debug(const char *format, ...);

#ifdef DEBUG
# define debug(...) __debug(__VA_ARGS__)
#else
# define debug(...)
#endif
