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
#include <sys/cdefs.h>

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
	for (int i = 0; i < context->n_free_rules; i++) {
		free(context->free_rules[i]->free_name);
		free(context->free_rules[i]);
	}

	for (int i = 0; i < context->n_locals; i++)
		free(context->local_names[i]);

	free(context->locals);
	free(context->local_names);
	free(context->free_rules);
}

static void __unused fn_add_free_rule(fn_context_t *context, free_rule_t *rule)
{
	context->free_rules =
	    realloc(context->free_rules,
		    sizeof(free_rule_t *) * (context->n_free_rules + 1));
	context->free_rules[context->n_free_rules++] = rule;
}

static LLVMTypeRef gen_str_type(LLVMModuleRef mod)
{
	return LLVMGetTypeByName2(LLVMGetModuleContext(mod), "str");
}

static LLVMTypeRef gen_plain_type(LLVMModuleRef mod, plain_type t)
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
		return gen_str_type(mod);
	default:
		return LLVMVoidType();
	}
}

LLVMTypeRef gen_type(LLVMModuleRef mod, type_t *ty)
{
	if (ty->type == TY_PLAIN)
		return gen_plain_type(mod, ty->v_plain);
	if (ty->type == TY_POINTER)
		return LLVMPointerType(gen_type(mod, ty->v_base), 0);
	if (ty->type == TY_ARRAY)
		return LLVMArrayType(gen_type(mod, ty->v_base), ty->len);
	if (ty->type == TY_OBJECT)
		error("LLVM object type is not yet implemented");
	return LLVMVoidType();
}

LLVMTypeRef gen_function_type(LLVMModuleRef mod, fn_expr_t *func)
{
	LLVMTypeRef *param_types;
	LLVMTypeRef func_type;

	param_types = calloc(func->n_params, sizeof(LLVMTypeRef));

	for (int i = 0; i < func->n_params; i++)
		param_types[i] = gen_type(mod, func->params[i]->type);

	func_type = LLVMFunctionType(gen_type(mod, func->return_type),
				     param_types, func->n_params, 0);
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
	error("emit: gen_addr for non-alloca var");
	return NULL;
}

static LLVMValueRef gen_deref(LLVMBuilderRef builder, fn_context_t *context,
			      const char *name, LLVMTypeRef deref_to)
{
	LLVMValueRef local = fn_find_local(context, name);

	/* If local is a pointer, we need to dereference it twice, because we
	   alloca the pointer too. */
	var_decl_expr_t *var = node_resolve_local(context->func, name, 0);
	if (var->type->type == TY_POINTER) {
		return LLVMBuildLoad2(
		    builder, deref_to,
		    LLVMBuildLoad2(builder,
				   gen_type(context->llvm_mod, var->type),
				   local, ""),
		    "");
	}

	return LLVMBuildLoad2(builder, deref_to, local, "");
}

static LLVMValueRef gen_literal_value(LLVMBuilderRef builder,
				      fn_context_t *context,
				      literal_expr_t *lit)
{
	if (lit->type->v_plain == PT_I32)
		return LLVMConstInt(LLVMInt32Type(), lit->v_i32, false);
	if (lit->type->v_plain == PT_I64)
		return LLVMConstInt(LLVMInt64Type(), lit->v_i64, false);

	/* cf_strset */
	if (lit->type->v_plain == PT_STR) {
		LLVMValueRef str;
		LLVMValueRef func;
		LLVMValueRef init;

		str = LLVMBuildAlloca(builder, gen_str_type(context->llvm_mod),
				      "");

		init = LLVMBuildInsertValue(
		    builder, LLVMGetUndef(gen_str_type(context->llvm_mod)),
		    LLVMConstInt(LLVMInt32Type(), 0, false), 2, "");
		LLVMBuildStore(builder, init, str);

		func = LLVMGetNamedFunction(context->llvm_mod, "cf_strset");
		if (!func)
			error("emit: missing `cf_strset` builtin function");

		LLVMValueRef args[3];

		args[0] = str;
		args[1] = LLVMBuildGlobalStringPtr(builder, lit->v_str.ptr, "");
		args[2] = LLVMConstInt(LLVMInt64Type(), lit->v_str.len, false);

		fn_candidates_t *results =
		    module_find_fn_candidates(context->module, "cf_strset");
		if (!results->n_candidates)
			error("emit: missing `cf_strset` builtin function");

		LLVMBuildCall2(
		    builder,
		    gen_function_type(context->llvm_mod, results->candidate[0]),
		    func, args, 3, "");

		free(results->candidate);
		free(results);

		return LLVMBuildLoad2(builder, gen_str_type(context->llvm_mod),
				      str, "");
	}

	error("emit: could not generate literal");
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
		return gen_literal_value(builder, context, value->literal);
	} else if (value->type == VE_CALL) {
		return emit_call_node(builder, context, value->call);
	} else if (value->type == VE_DEREF) {
		return gen_deref(
		    builder, context, value->name,
		    gen_type(context->llvm_mod, value->return_type));
	} else {
		if (!value->left || !value->right) {
			error("emit: undefined value: two-side op with only "
			      "one side defined");
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
	for (int i = 0; i < context->n_locals; i++) {
		if (!strcmp(context->local_names[i], name))
			return context->locals[i];
	}

	error("could not find local `%s` in `%s`", name,
	      E_AS_FN(context->func->data)->name);
}

void emit_function_decl(LLVMModuleRef mod, mod_expr_t *module, fn_expr_t *fn,
			LLVMLinkage linkage)
{
	LLVMValueRef f;
	char *ident;

	ident = fn->flags & FN_NOMANGLE ? fn->name : nxg_mangle(fn);
	f = LLVMAddFunction(mod, ident, gen_function_type(mod, fn));
	LLVMSetLinkage(f, linkage);

	if (!(fn->flags & FN_NOMANGLE))
		free(ident);
}

void emit_free_call(LLVMBuilderRef builder, fn_context_t *context,
		    free_rule_t *rule)
{
	LLVMValueRef func;

	func = LLVMGetNamedFunction(context->llvm_mod, rule->free_name);
	if (!func)
		error("emit: missing `%s` free builtin function",
		      rule->free_name);

	LLVMValueRef arg = rule->value;

	fn_candidates_t *results =
	    module_find_fn_candidates(context->module, rule->free_name);
	if (!results->n_candidates)
		error("emit: missing `%s` free function", rule->free_name);

	LLVMBuildCall2(
	    builder,
	    gen_function_type(context->llvm_mod, results->candidate[0]), func,
	    &arg, 1, "");

	free(results->candidate);
	free(results);
}

void emit_return_node(LLVMBuilderRef builder, fn_context_t *context,
		      expr_t *node)
{
	LLVMValueRef ret;
	value_expr_t *value;

	value = node->data;

	/* Free allocated resources */
	for (int i = 0; i < context->n_free_rules; i++)
		emit_free_call(builder, context, context->free_rules[i]);

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

	/* If we assign to a dereference, we first need to get the value under
	   the alloca. */
	if (data->to->type == VE_DEREF) {
		LLVMValueRef local;
		LLVMValueRef ptr;
		type_t *ptr_type = type_pointer_of(data->to->return_type);

		local = fn_find_local(context, data->to->name);
		if (LLVMIsAAllocaInst(local)) {
			ptr = LLVMBuildLoad2(
			    builder, gen_type(context->llvm_mod, ptr_type),
			    gen_addr(builder, context, data->to->name), "");
		} else {
			ptr = local;
		}

		LLVMBuildStore(
		    builder, gen_new_value(builder, context, data->value), ptr);
		type_destroy(ptr_type);
		return;
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
			args[i] = gen_literal_value(builder, context,
						    call->args[i]->literal);
		} else if (call->args[i]->type == VE_REF) {
			args[i] = gen_local_value(builder, context,
						  call->args[i]->name);
		} else if (call->args[i]->type == VE_PTR) {
			args[i] =
			    gen_addr(builder, context, call->args[i]->name);
		} else if (call->args[i]->type == VE_DEREF) {
			args[i] =
			    gen_deref(builder, context, call->args[i]->name,
				      gen_type(context->llvm_mod,
					       call->args[i]->return_type));
		} else if (call->args[i]->type == VE_CALL) {
			args[i] = emit_call_node(builder, context,
						 call->args[i]->call);
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
	ret = LLVMBuildCall2(builder,
			     gen_function_type(context->llvm_mod, call->func),
			     func, args, n_args, "");

	free(args);

	if (!(call->func->flags & FN_NOMANGLE))
		free(name);

	return ret;
}

void emit_var_decl(LLVMBuilderRef builder, fn_context_t *context, expr_t *node)
{
	LLVMValueRef alloca;

	alloca = LLVMBuildAlloca(
	    builder, gen_type(context->llvm_mod, E_AS_VDECL(node->data)->type),
	    "");

	fn_context_add_local(context, alloca, E_AS_VDECL(node->data)->name);
}

void emit_node(LLVMBuilderRef builder, fn_context_t *context, expr_t *node)
{
	switch (node->type) {
	case E_VARDECL:
		emit_var_decl(builder, context, node);
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

void emit_function_body(LLVMModuleRef mod, expr_t *module, expr_t *fn)
{
	LLVMValueRef func;
	fn_context_t context;
	char *name;

	name = nxg_mangle(fn->data);
	func = LLVMGetNamedFunction(mod, name);

	memset(&context, 0, sizeof(context));
	context.func = fn;
	context.module = module;
	context.llvm_func = func;
	context.llvm_mod = mod;

	LLVMBasicBlockRef args_block, code_block;
	LLVMBuilderRef builder;
	expr_t *walker;

	args_block = LLVMAppendBasicBlock(func, "args");
	builder = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(builder, args_block);

	/* We want to move all copied arguments into alloca values.  */
	fn_expr_t *data = fn->data;
	for (int i = 0; i < data->n_params; i++) {
		LLVMValueRef arg;

		if (data->params[i]->type->type == TY_POINTER) {
			arg = LLVMGetParam(func, i);
		} else {
			arg = LLVMBuildAlloca(
			    builder, gen_type(mod, data->params[i]->type), "");
			LLVMBuildStore(builder, LLVMGetParam(func, i), arg);
		}

		fn_context_add_local(&context, arg, data->params[i]->name);
	}

	code_block = LLVMAppendBasicBlock(func, "code");
	LLVMBuildBr(builder, code_block);

	LLVMPositionBuilderAtEnd(builder, code_block);

	walker = fn->child;
	while (walker) {
		emit_node(builder, &context, walker);
		walker = walker->next;
	}

	if (!data->n_params)
		LLVMDeleteBasicBlock(args_block);

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

static LLVMModuleRef emit_module_contents(LLVMModuleRef mod, expr_t *module)
{
	mod_expr_t *mod_data;
	char *err_msg = NULL;
	expr_t *walker;

	mod_data = E_AS_MOD(module->data);

	/* Declare extern functions. */
	for (int i = 0; i < mod_data->n_decls; i++) {
		emit_function_decl(mod, module->data, mod_data->decls[i],
				   LLVMExternalLinkage);
	}

	/* For emitting functions we need to make 2 passes. The first time we
	   declare all functions as private, and only then during the second
	   pass we may implement the function bodies. */

	walker = module->child;
	while (walker) {
		if (walker->type == E_FUNCTION) {
			emit_function_decl(mod, module->data,
					   E_AS_FN(walker->data),
					   LLVMInternalLinkage);
		} else {
			error("cannot emit anything other than a "
			      "function");
		}
		walker = walker->next;
	}

	walker = module->child;
	while (walker) {
		if (walker->type == E_FUNCTION)
			emit_function_body(mod, module, walker);
		walker = walker->next;
	}

	if (LLVMVerifyModule(mod, LLVMPrintMessageAction, &err_msg)) {
		error("LLVM failed to generate the `%s` module",
		      mod_data->name);
	}

	LLVMDisposeMessage(err_msg);
	return mod;
}

static void add_builtin_types(LLVMModuleRef module)
{
	LLVMTypeRef str;
	LLVMTypeRef fields[3];

	fields[0] = LLVMInt64Type();
	fields[1] = LLVMPointerType(LLVMInt8Type(), 0);
	fields[2] = LLVMInt32Type();

	str = LLVMStructCreateNamed(LLVMGetModuleContext(module), "str");
	LLVMStructSetBody(str, fields, 3, false);
}

void emit_module(expr_t *module, const char *out, bool is_main)
{
	LLVMModuleRef mod;
	mod_expr_t *mod_data;

	mod_data = module->data;
	mod = LLVMModuleCreateWithName(mod_data->name);
	LLVMSetSourceFileName(mod, mod_data->source_name,
			      strlen(mod_data->source_name));

	add_builtin_types(mod);

	for (int i = 0; i < mod_data->n_imported; i++)
		emit_module_contents(mod, mod_data->imported[i]);
	emit_module_contents(mod, module);

	emit_main_function(mod);

	LLVMPrintModuleToFile(mod, out, NULL);
	LLVMDisposeModule(mod);
}
