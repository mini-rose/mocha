#pragma once

#include <nxg/parser.h>

char *mangle(fn_expr_t *func);
void emit_module(expr_t *module, const char *out);
