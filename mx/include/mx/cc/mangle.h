/* mangle.h - function for mangling names
   Copyright (c) 2023 mini-rose */

#pragma once
#include <mx/cc/parser.h>

char *mx_mangle(const fn_expr_t *func);
char *mx_mangle_type(type_t *ty, char *buf);
