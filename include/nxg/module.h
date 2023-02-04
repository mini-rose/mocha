#pragma once

#include <nxg/parser.h>

fn_expr_t *module_add_decl(expr_t *module);
fn_expr_t *module_find_fn(expr_t *module, char *name);
