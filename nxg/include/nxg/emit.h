#pragma once

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <nxg/parser.h>

typedef struct
{
	expr_t *module;
	expr_t *func;
	LLVMModuleRef llvm_mod;
	LLVMValueRef llvm_func;
	LLVMValueRef *locals;
	int n_locals;
} fn_context_t;

void emit_module(expr_t *module, const char *out, bool is_main);
