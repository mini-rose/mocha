#pragma once

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <nxg/cc/parser.h>

typedef struct
{
	LLVMValueRef value;
	char *free_name;
} free_rule_t;

typedef struct
{
	expr_t *module;
	expr_t *func;
	LLVMModuleRef llvm_mod;
	LLVMValueRef llvm_func;
	LLVMValueRef *locals;
	char **local_names;
	int n_locals;
	free_rule_t **free_rules;
	int n_free_rules;
} fn_context_t;

void emit_module(expr_t *module, const char *out, bool is_main);
