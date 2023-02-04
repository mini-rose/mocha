#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <nxg/emit.h>
#include <nxg/error.h>
#include <nxg/nxg.h>
#include <nxg/parser.h>
#include <nxg/type.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char plain_type_mid(plain_type t)
{
	const char type_mangle_ids[] = {
	    [PT_NULL] = 'n', [PT_BOOL] = 'b', [PT_I8] = 'c',   [PT_U8] = 'C',
	    [PT_I16] = 's',  [PT_U16] = 'S',  [PT_I32] = 'i',  [PT_U32] = 'I',
	    [PT_I64] = 'l',  [PT_U64] = 'L',  [PT_I128] = 'q', [PT_U128] = 'Q',
	    [PT_F32] = 'f',  [PT_F64] = 'd',  [PT_STR] = 's',  [PT_PTR] = 'p'};
	const int n = sizeof(type_mangle_ids);
	if (t >= 0 && t < n)
		return type_mangle_ids[t];
	return 'x';
}

char *mangle(fn_expr_t *func)
{
	char *name = calloc(128, 4);
	snprintf(name, 512, "cf.%s", func->name);
	for (int i = 0; i < func->n_params; i++) {
		char id = plain_type_mid(func->params[i]->type);
		name[strlen(name)] = id;
	}
	return name;
}

LLVMTypeRef gen_plain_type(plain_type t)
{
	switch (t) {
	case PT_U8:
	case PT_I8:
		return LLVMInt8Type();
	case PT_U16:
	case PT_I16:
		return LLVMInt16Type();
	case PT_U32:
	case PT_I32:
		return LLVMInt32Type();
	case PT_U64:
	case PT_I64:
		return LLVMInt64Type();
	case PT_U128:
	case PT_I128:
		return LLVMInt64Type();
	case PT_F32:
		return LLVMFloatType();
	case PT_F64:
		return LLVMDoubleType();
	case PT_BOOL:
		return LLVMInt1Type();
	default:
		return LLVMVoidType();
	}
}

LLVMTypeRef gen_function_type(fn_expr_t *func)
{
	LLVMTypeRef *param_types;
	LLVMTypeRef func_type;

	param_types = calloc(func->n_params, sizeof(LLVMTypeRef));

	for (int i = 0; i < func->n_params; i++)
		param_types[i] = gen_plain_type(func->params[i]->type);

	func_type = LLVMFunctionType(gen_plain_type(func->return_type),
				     param_types, func->n_params, 0);
	free(param_types);

	return func_type;
}

void emit_return_node(LLVMBuilderRef builder, expr_t *node)
{
	LLVMValueRef llvm_val;
	value_expr_t *value;

	value = node->data;
	if (value->type != VE_LIT) {
		warning("emit_return_node: we don't know how to emit this "
			"return type");
		return;
	}

	if (value->return_type == PT_NULL) {
		LLVMBuildRetVoid(builder);
	} else {
		llvm_val =
		    LLVMConstInt(gen_plain_type(value->return_type), 0, false);
		LLVMBuildRet(builder, llvm_val);
	}
}

void emit_node(LLVMBuilderRef builder, expr_t *node)
{

	switch (node->type) {
	case E_VARDECL:
		LLVMBuildAlloca(builder,
				gen_plain_type(E_AS_VDECL(node->data)->type),
				E_AS_VDECL(node->data)->name);
		break;
	case E_RETURN:
		emit_return_node(builder, node);
		break;
	default:
		warning("undefined emit rules for node");
	}
}

void emit_function(LLVMModuleRef mod, expr_t *node)
{
	LLVMTypeRef func_type;
	LLVMValueRef func;
	char *name;

	func_type = gen_function_type(node->data);
	name = mangle(node->data);
	func = LLVMAddFunction(mod, name, func_type);

	LLVMBasicBlockRef start_block;
	LLVMBuilderRef builder;
	expr_t *walker;

	start_block = LLVMAppendBasicBlock(func, "start");
	builder = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(builder, start_block);

	walker = node->child;
	while (walker) {
		emit_node(builder, walker);
		walker = walker->next;
	}

	LLVMDisposeBuilder(builder);
	free(name);
}

void emit_main_function(LLVMModuleRef mod)
{
	LLVMTypeRef param_types[2];
	LLVMValueRef return_value;
	LLVMValueRef func;

	param_types[0] = LLVMInt32Type();
	param_types[1] = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);

	func = LLVMAddFunction(
	    mod, "main",
	    LLVMFunctionType(LLVMInt32Type(), param_types, 2, false));

	LLVMBasicBlockRef start_block;
	LLVMBuilderRef builder;

	start_block = LLVMAppendBasicBlock(func, "start");
	builder = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(builder, start_block);

	LLVMBuildCall2(builder, LLVMFunctionType(LLVMVoidType(), NULL, 0, 0),
		       LLVMGetNamedFunction(mod, "cf.main"), NULL, 0, "");

	return_value = LLVMConstNull(LLVMInt32Type());
	LLVMBuildRet(builder, return_value);

	LLVMDisposeBuilder(builder);
}

void emit_module(expr_t *module, const char *out)
{
	LLVMModuleRef mod;
	mod_expr_t *mod_data;
	char *err_msg = NULL;

	mod_data = E_AS_MOD(module->data);
	mod = LLVMModuleCreateWithName(mod_data->name);
	LLVMSetSourceFileName(mod, mod_data->source_name,
			      strlen(mod_data->source_name));

	expr_t *walker = module->child;

	while (walker) {
		if (walker->type == E_FUNCTION)
			emit_function(mod, walker);
		else
			error("cannot emit anything other than a function");
		walker = walker->next;
	}

	/* if we're in the main module, add a main function */
	if (!strcmp(mod_data->name, MAIN_MODULE))
		emit_main_function(mod);

	LLVMPrintModuleToFile(mod, out, &err_msg);
	LLVMDisposeMessage(err_msg);
	LLVMDisposeModule(mod);
}
