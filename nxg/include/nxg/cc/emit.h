#pragma once

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <nxg/cc/parser.h>

typedef struct
{
	LLVMValueRef value;
	type_t *type;
	char *drop;
} auto_drop_rule_t;

typedef struct
{
	expr_t *module;
	expr_t *func;
	LLVMModuleRef llvm_mod;
	LLVMValueRef llvm_func;
	LLVMValueRef *locals;
	char **local_names;
	int n_locals;
	auto_drop_rule_t **auto_drops;
	int n_auto_drops;
} fn_context_t;

void emit_module(expr_t *module, const char *out, bool is_main);
