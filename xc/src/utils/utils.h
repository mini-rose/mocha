/* utils.h - random utilities
   Copyright (c) 2023 mini-rose */

#pragma once

#include "../opt.h"

#define LEN(array) (sizeof(array) / sizeof(*array))

void debug_impl(const char *func, const char *fmt, ...);

#ifdef OPT_DEBUG_INFO
# define debug(...) debug_impl(__func__, __VA_ARGS__)
#else
# define debug(...)
#endif
