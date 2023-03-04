/* utils/error.h
   Copyright (c) 2023 mini-rose */

#pragma once

#include <stdnoreturn.h>

noreturn void error(const char *format, ...);
void warning(const char *format, ...);
