/* Results type from resolve functions.
   Copyright (c) 2023 mini-rose */

#pragma once

typedef struct fn_expr fn_expr_t;

typedef struct
{
	fn_expr_t **candidate;
	int n_candidates;
} fn_candidates_t;

void fn_add_candidate(fn_candidates_t *resolved, fn_expr_t *ptr);
