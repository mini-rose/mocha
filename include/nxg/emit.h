#pragma once

#include <llvm-c/Types.h>
#include <nxg/parser.h>

typedef struct
{
	expr_t *module;
	expr_t *func;
	LLVMValueRef llvm_func;
	LLVMValueRef *locals;
	int n_locals;
} fn_context_t;

char *mangle(fn_expr_t *func);
void emit_module(expr_t *module, const char *out);
