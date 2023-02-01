#include "nxg/parser.h"
#include "nxg/tokenize.h"

#include <llvm-c/Core.h>
#include <nxg/error.h>
#include <nxg/generate.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LEN(array) sizeof(array) / sizeof(array[0])

LLVMTypeRef gen_str_type(int length)
{
	LLVMTypeRef i8Type = LLVMInt8Type();
	return LLVMArrayType(i8Type, length);
}

LLVMTypeRef gen_type(plain_type type, int length)
{
	if (length)
		return gen_str_type(length);

	switch (type) {
	case T_VOID: /* fallthrough */
	case T_NULL:
		return LLVMVoidType();
	case T_I8:
		return LLVMInt8Type();
	case T_I16:
		return LLVMInt16Type();
	case T_I32:
		return LLVMInt32Type();
	case T_I64:
		return LLVMInt64Type();
	case T_I128:
		return LLVMInt128Type();
	case T_F32:
		return LLVMFloatType();
	case T_F64:
		return LLVMDoubleType();
	case T_BOOL:
		return LLVMInt1Type();
	case T_STR:
		break;
	};

	return NULL;
}

LLVMTypeRef gen_function(plain_type ret_type, int length, int nargs, arg_t **args)
{
	LLVMTypeRef params[nargs];

	for (int i = 0; i < nargs; i++) {
		params[i] = gen_type(args[i]->type, 0);
	}

	return LLVMFunctionType(gen_type(ret_type, length), params, nargs, 0);
}

expr_t *next_expr(expr_t *ast)
{
	static expr_t *expr = NULL;

	if (expr == NULL)
		return expr = ast;

	return expr = expr->next;
}

void generate(expr_t *ast)
{
	expr_t *expr = next_expr(ast);
	LLVMModuleRef mod = LLVMModuleCreateWithName(E_AS_MOD(ast->data)->name);

	expr = next_expr(NULL);

	while (expr && expr->type == E_FUNCTION) {
		fn_expr_t *fn = E_AS_FN(expr->data);
		LLVMTypeRef fn_type =
		    gen_function(fn->return_type, 0, fn->n_args, fn->args);
		LLVMValueRef fn_val = LLVMAddFunction(mod, fn->name, fn_type);
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn_val, "entry");
		LLVMBuilderRef builder = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(builder, entry);

		expr_t *next = expr->next;

		while (next) {
			switch (next->type) {
			case E_MODULE:
			case E_FUNCTION:
				break;
			case E_CALL:
				break;
			case E_VARDEF:
				break;
			case E_VARDECL:
				LLVMBuildAlloca(
				    builder,
				    gen_type(E_AS_VDECL(next->data)->type, 0),
				    E_AS_VDECL(next->data)->name);
				break;
			case E_ASSIGN:
				break;
			case E_RETURN:
				if (E_AS_RETURN(next->data)->type == T_VOID
				    || E_AS_RETURN(next->data)->type == T_NULL)
					LLVMBuildRetVoid(builder);
			}

			next = next->child;
		}

		LLVMDisposeBuilder(builder);
		expr = next_expr(expr->next);
	};

	char *error = NULL;
	LLVMPrintModuleToFile(mod, "example.ll", &error);
	LLVMDisposeMessage(error);
	LLVMDisposeModule(mod);
}
