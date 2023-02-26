/* nxg/mangle.h - function for mangling names
   Copyright (c) 2023 mini-rose */

#pragma once
#include <nxg/cc/parser.h>

char *nxg_mangle(const fn_expr_t *func);
char *nxg_mangle_type(type_t *ty, char *buf);
