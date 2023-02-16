#pragma once

#include <nxg/cc/parser.h>

typedef struct
{
	fn_expr_t **candidate;
	int n_candidates;
} fn_candidates_t;

mod_expr_t *module_import(settings_t *settings, expr_t *module, char *file);
mod_expr_t *module_std_import(settings_t *settings, expr_t *module, char *file);
fn_expr_t *module_add_decl(expr_t *module);
fn_expr_t *module_add_local_decl(expr_t *module);
type_t *module_add_type_decl(expr_t *module);
fn_candidates_t *module_find_fn_candidates(expr_t *module, char *name);
