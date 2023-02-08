#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <llvm-c/blake3.h>
#include <nxg/cc/emit.h>
#include <nxg/cc/mangle.h>
#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/type.h>
#include <nxg/nxg.h>
#include <nxg/utils/error.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LLVMValueRef emit_call_node(LLVMBuilderRef builder,
				   fn_context_t *context, call_expr_t *call);
static LLVMValueRef fn_find_local(fn_context_t *context, const char *name);

static void fn_context_add_local(fn_context_t *context, LLVMValueRef ref,
				 char *name)
{
	context->locals = realloc(
	    context->locals, sizeof(LLVMValueRef) * (context->n_locals + 1));
	context->local_names = realloc(
	    context->local_names, sizeof(char *) * (context->n_locals + 1));

	context->locals[context->n_locals] = ref;
	context->local_names[context->n_locals++] = strdup(name);
}

static void fn_context_destroy(fn_context_t *context)
{
	free(context->locals);
	for (int i = 0; i < context->n_locals; i++)
		free(context->local_names[i]);
	free(context->local_names);
}

static LLVMTypeRef gen_plain_type(plain_type t)
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
	case PT_STR:
		error("LLVM str type is not yet implemented");
	default:
		return LLVMVoidType();
	}
}

LLVMTypeRef gen_type(type_t *ty)
{
	if (ty->type == TY_PLAIN)
		return gen_plain_type(ty->v_plain);
	if (ty->type == TY_POINTER)
		return LLVMPointerType(gen_type(ty->v_base), 0);
	if (ty->type == TY_ARRAY)
		return LLVMArrayType(gen_type(ty->v_base), ty->len);
	if (ty->type == TY_OBJECT)
		error("LLVM object type is not yet implemented");
	return LLVMVoidType();
}

LLVMTypeRef gen_function_type(fn_expr_t *func)
{
	LLVMTypeRef *param_types;
	LLVMTypeRef func_type;

	param_types = calloc(func->n_params, sizeof(LLVMTypeRef));

	for (int i = 0; i < func->n_params; i++)
		param_types[i] = gen_type(func->params[i]->type);

	func_type = LLVMFunctionType(gen_type(func->return_type), param_types,
				     func->n_params, 0);
	free(param_types);

	return func_type;
}

static LLVMValueRef gen_local_value(LLVMBuilderRef builder,
				    fn_context_t *context, const char *name)
{
	LLVMValueRef val = fn_find_local(context, name);
	if (LLVMIsAAllocaInst(val)) {
		val =
		    LLVMBuildLoad2(builder, LLVMGetAllocatedType(val), val, "");
	}
	return val;
}

static LLVMValueRef gen_addr(LLVMBuilderRef builder, fn_context_t *context,
			     const char *name)
{
	LLVMValueRef local = fn_find_local(context, name);
	if (LLVMIsAAllocaInst(local))
		return local;
	return NULL;
}

static LLVMValueRef gen_deref(LLVMBuilderRef builder, fn_context_t *context,
			      const char *name, LLVMTypeRef deref_to)
{
	LLVMValueRef local = fn_find_local(context, name);
	return LLVMBuildLoad2(builder, deref_to, local, "");
}

static LLVMValueRef gen_literal_value(literal_expr_t *lit)
{
	if (lit->type->v_plain == PT_I32)
		return LLVMConstInt(LLVMInt32Type(), lit->v_i32, false);

	return NULL;
}

/**
 * Generate a new value in the block from the value expression given. It can be
 * a reference to a variable, a literal value, a call or two-hand-side
 * operation.
 */
static LLVMValueRef gen_new_value(LLVMBuilderRef builder, fn_context_t *context,
				  value_expr_t *value)
{
	if (value->type == VE_NULL) {
		error("tried to generate a null value");
	} else if (value->type == VE_REF) {
		return gen_local_value(builder, context, value->name);
	} else if (value->type == VE_LIT) {
		return gen_literal_value(value->literal);
	} else if (value->type == VE_CALL) {
		return emit_call_node(builder, context, value->call);
	} else {
		if (!value->left || !value->right) {
			error("undefined value: two-side op with only one side "
			      "defined");
		}

		LLVMValueRef new, left, right;

		new = NULL;
		left = gen_new_value(builder, context, value->left);
		right = gen_new_value(builder, context, value->right);

		switch (value->type) {
		case VE_ADD:
			new = LLVMBuildAdd(builder, left, right, "");
			break;
		case VE_SUB:
			new = LLVMBuildSub(builder, left, right, "");
			break;
		case VE_MUL:
			new = LLVMBuildMul(builder, left, right, "");
			break;
		default:
			error("unknown operation: %s", value_expr_type_name(value->type));
		}

		return new;
	}
}

static LLVMValueRef fn_find_local(fn_context_t *context, const char *name)
{
	for (int i = 0; i < context->n_locals; i++)
		if (!strcmp(context->local_names[i], name))
			return context->locals[i];

	error("could not find local `%s` in `%s`", name,
	      E_AS_FN(context->func->data)->name);
}

void emit_function_decl(LLVMModuleRef mod, fn_expr_t *fn)
{
	LLVMValueRef f;
	char *ident;

	ident = fn->flags & FN_NOMANGLE ? fn->name : nxg_mangle(fn);
	f = LLVMAddFunction(mod, ident, gen_function_type(fn));
	LLVMSetLinkage(f, LLVMExternalLinkage);

	if (!(fn->flags & FN_NOMANGLE))
		free(ident);
}

void emit_return_node(LLVMBuilderRef builder, fn_context_t *context,
		      expr_t *node)
{
	LLVMValueRef ret;
	value_expr_t *value;

	value = node->data;

	if (value->type == VE_NULL) {
		LLVMBuildRetVoid(builder);
	} else {
		ret = gen_new_value(builder, context, node->data);
		if (!ret)
			error("could not generate return value for %s",
			      E_AS_FN(context->func->data)->name);
		LLVMBuildRet(builder, ret);
	}
}

static void emit_assign_node(LLVMBuilderRef builder, fn_context_t *context,
			     expr_t *node)
{
	assign_expr_t *data;

	data = node->data;

	/* Check for matching types on left & right side. */
	if (!type_cmp(data->to->return_type, data->value->return_type)) {
		error("mismatched types on left and right side of assignment "
		      "to %s",
		      data->to->name);
	}

	/* Make a temporary for the object we want to store. */
	LLVMBuildStore(builder, gen_new_value(builder, context, data->value),
		       gen_addr(builder, context, data->to->name));
}

static LLVMValueRef emit_call_node(LLVMBuilderRef builder,
				   fn_context_t *context, call_expr_t *call)
{
	LLVMValueRef *args;
	LLVMValueRef func;
	LLVMValueRef ret;
	char *name;
	int n_args;

	args = calloc(call->n_args, sizeof(LLVMValueRef));
	n_args = call->n_args;

	for (int i = 0; i < n_args; i++) {
		/* literal argument */
		if (call->args[i]->type == VE_LIT) {
			args[i] = gen_literal_value(call->args[i]->literal);
		} else if (call->args[i]->type == VE_REF) {
			args[i] = gen_local_value(builder, context,
						  call->args[i]->name);
		} else if (call->args[i]->type == VE_PTR) {
			args[i] =
			    gen_addr(builder, context, call->args[i]->name);
		} else if (call->args[i]->type == VE_DEREF) {
			args[i] =
			    gen_deref(builder, context, call->args[i]->name,
				      gen_type(call->args[i]->return_type));
		} else {
			error("cannot emit the %d argument to a call to `%s`",
			      i + 1, call->name);
		}
	}

	name = call->func->name;
	if (!(call->func->flags & FN_NOMANGLE))
		name = nxg_mangle(call->func);
	func = LLVMGetNamedFunction(context->llvm_mod, name);
	if (!func)
		error("missing named func %s", name);
	ret = LLVMBuildCall2(builder, gen_function_type(call->func), func, args,
			     n_args, "");

	free(args);

	if (!(call->func->flags & FN_NOMANGLE))
		free(name);

	return ret;
}

void emit_node(LLVMBuilderRef builder, fn_context_t *context, expr_t *node)
{
	switch (node->type) {
	case E_VARDECL:
		fn_context_add_local(
		    context,
		    LLVMBuildAlloca(builder,
				    gen_type(E_AS_VDECL(node->data)->type), ""),
		    E_AS_VDECL(node->data)->name);
		break;
	case E_RETURN:
		emit_return_node(builder, context, node);
		break;
	case E_ASSIGN:
		emit_assign_node(builder, context, node);
		break;
	case E_CALL:
		emit_call_node(builder, context, node->data);
		break;
	case E_SKIP:
		break;
	default:
		warning("undefined emit rules for node");
	}
}

void emit_function(LLVMModuleRef mod, expr_t *module, expr_t *fn)
{
	LLVMTypeRef func_type;
	LLVMValueRef func;
	fn_context_t context;
	char *name;

	func_type = gen_function_type(fn->data);
	name = nxg_mangle(fn->data);
	func = LLVMAddFunction(mod, name, func_type);

	memset(&context, 0, sizeof(context));
	context.func = fn;
	context.module = module;
	context.llvm_func = func;
	context.llvm_mod = mod;

	LLVMBasicBlockRef start_block;
	LLVMBuilderRef builder;
	expr_t *walker;

	start_block = LLVMAppendBasicBlock(func, "start");
	builder = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(builder, start_block);

	/* We want to move all arguments into alloca values.  */
	fn_expr_t *data = fn->data;
	for (int i = 0; i < data->n_params; i++) {
		LLVMValueRef arg = LLVMBuildAlloca(
		    builder, gen_type(data->params[i]->type), "");
		LLVMBuildStore(builder, LLVMGetParam(func, i), arg);
		fn_context_add_local(&context, arg, data->params[i]->name);
	}

	walker = fn->child;
	while (walker) {
		emit_node(builder, &context, walker);
		walker = walker->next;
	}

	if (LLVMVerifyFunction(func, LLVMPrintMessageAction)) {
		error("something is wrong with the emitted `%s` function",
		      name);
	}

	LLVMDisposeBuilder(builder);
	fn_context_destroy(&context);
	free(name);
}

void emit_main_function(LLVMModuleRef mod)
{
	LLVMTypeRef param_types[2];
	LLVMValueRef return_value;
	LLVMValueRef func;

	param_types[0] = LLVMInt32Type();
	param_types[1] = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);

	/**
	 * int main(int argc, char **argv)
	 * {
	 *      cf.main();
	 * }
	 */

	func = LLVMAddFunction(
	    mod, "main",
	    LLVMFunctionType(LLVMInt32Type(), param_types, 2, false));

	LLVMBasicBlockRef start_block;
	LLVMBuilderRef builder;

	start_block = LLVMAppendBasicBlock(func, "start");
	builder = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(builder, start_block);

	LLVMBuildCall2(builder, LLVMFunctionType(LLVMVoidType(), NULL, 0, 0),
		       LLVMGetNamedFunction(mod, "_C4main"), NULL, 0, "");

	return_value = LLVMConstNull(LLVMInt32Type());
	LLVMBuildRet(builder, return_value);

	LLVMDisposeBuilder(builder);
}

void emit_module(expr_t *module, const char *out, bool is_main)
{
	LLVMModuleRef mod;
	mod_expr_t *mod_data;
	char *err_msg = NULL;

	mod_data = E_AS_MOD(module->data);
	mod = LLVMModuleCreateWithName(mod_data->name);
	LLVMSetSourceFileName(mod, mod_data->source_name,
			      strlen(mod_data->source_name));

	/* declare extern */
	for (int i = 0; i < mod_data->n_decls; i++)
		emit_function_decl(mod, mod_data->decls[i]);

	expr_t *walker = module->child;

	while (walker) {
		if (walker->type == E_FUNCTION)
			emit_function(mod, module, walker);
		else
			error("cannot emit anything other than a "
			      "function");
		walker = walker->next;
	}

	/* if we're in the main module, add a main function */
	if (is_main)
		emit_main_function(mod);

	if (LLVMVerifyModule(mod, LLVMPrintMessageAction, &err_msg)) {
		error("something is wrong with the emitted `%s` module",
		      mod_data->name);
	}

	LLVMPrintModuleToFile(mod, out, &err_msg);
	LLVMDisposeMessage(err_msg);
	LLVMDisposeModule(mod);
}
