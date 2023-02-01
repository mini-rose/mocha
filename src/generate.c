#include "nxg/parser.h"
#include "nxg/tokenize.h"
#include <llvm-c/Core.h>
#include <nxg/generate.h>
#include <stdarg.h>
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

LLVMTypeRef gen_function(plain_type ret_type, int length, int nargs, ...)
{
	va_list args;
	LLVMTypeRef params[nargs];

	va_start(args, nargs);

	for (int i = 0; i < nargs; i++) {
		int type = va_arg(args, int);

		if (type == T_STRING)
			params[i] = gen_type(type, va_arg(args, int));
		else
			params[i] = gen_type(type, 0);
	}

	return LLVMFunctionType(gen_type(ret_type, length), params, nargs, 0);
}

void generate(expr_t *ast) { }
