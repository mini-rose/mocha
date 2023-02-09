#pragma once

#include <nxg/cc/parser.h>

typedef struct
{
	fn_expr_t **candidate;
	int n_candidates;
} fn_candidates_t;

fn_expr_t *module_add_decl(expr_t *module);
fn_expr_t *module_add_local_decl(expr_t *module);
fn_candidates_t *module_find_fn_candidates(expr_t *module, char *name);
