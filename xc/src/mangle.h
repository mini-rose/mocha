/* mangle.h - function for mangling names
   Copyright (c) 2023 mini-rose */

#pragma once
#include "parser.h"

char *xc_mangle(const fn_expr_t *func);
char *xc_mangle_type(type_t *ty, char *buf);
