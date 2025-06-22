/* xc/emit.c - emit LLVM IR from an AST
   Copyright (c) 2023 mini-rose */

#include "emit.h"

#include "alloc.h"
#include "mangle.h"
#include "module.h"
#include "parser.h"
#include "type.h"
#include "utils/error.h"
#include "utils/utils.h"
#include "xc.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

static LLVMValueRef emit_call_node(LLVMBuilderRef builder,
				   fn_context_t *context, call_expr_t *call);
static void emit_copy(LLVMBuilderRef builder, fn_context_t *context,
		      LLVMValueRef to, LLVMValueRef from, type_t *ty);
static LLVMValueRef gen_zero_value_for(LLVMBuilderRef builder,
				       LLVMModuleRef mod, type_t *ty);
static LLVMValueRef fn_find_local(fn_context_t *context, const char *name);
static void emit_stackpop(LLVMBuilderRef builder, LLVMModuleRef mod);
static void emit_stackpush(LLVMBuilderRef builder, LLVMModuleRef mod,
			   const char *symbol, const char *file);
static LLVMValueRef gen_ptr_to_member(LLVMBuilderRef builder,
				      fn_context_t *context,
				      value_expr_t *node);
static LLVMValueRef gen_new_value(LLVMBuilderRef builder, fn_context_t *context,
				  value_expr_t *value);
static void emit_node(LLVMBuilderRef builder, fn_context_t *context,
		      expr_t *node);
static void emit_drop(LLVMBuilderRef builder, fn_context_t *context,
		      auto_drop_rule_t *rule);
static LLVMTypeRef gen_type(LLVMModuleRef mod, type_t *ty);

static void fn_context_add_local(fn_context_t *context, LLVMValueRef ref,
				 char *name)
{
	context->locals =
	    realloc_ptr_array(context->locals, context->n_locals + 1);
	context->local_names =
	    realloc_ptr_array(context->local_names, context->n_locals + 1);

	context->locals[context->n_locals] = ref;
	context->local_names[context->n_locals++] = slab_strdup(name);
}

static void fn_add_auto_drop(fn_context_t *context, auto_drop_rule_t *rule)
{
	context->auto_drops =
	    realloc_ptr_array(context->auto_drops, context->n_auto_drops + 1);
	context->auto_drops[context->n_auto_drops++] = rule;
}

static LLVMTypeRef gen_str_type(LLVMModuleRef mod)
{
	return LLVMGetTypeByName2(LLVMGetModuleContext(mod), "str");
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
	default:
		return LLVMVoidType();
	}
}

static LLVMTypeRef gen_object_type(LLVMModuleRef mod, type_t *ty)
{
	LLVMTypeRef type = LLVMGetTypeByName(mod, ty->v_object->name);

	if (!type) {
		error("emit: failed to find %%%s struct in module",
		      ty->v_object->name);
	}

	return type;
}

static LLVMTypeRef gen_array_type(LLVMModuleRef mod, type_t *ty)
{
	LLVMTypeRef fields[2];
	LLVMTypeRef res;
	char *qualname;

	qualname = slab_alloc(1024);
	qualname[0] = '.';
	xc_mangle_type(ty, qualname + 1);

	res = LLVMGetTypeByName(mod, qualname);
	if (res)
		return res;

	/* Create the array type */
	fields[0] = LLVMPointerType(gen_type(mod, ty->v_base), 0);
	fields[1] = LLVMInt64Type();

	res = LLVMStructCreateNamed(LLVMGetModuleContext(mod), qualname);
	LLVMStructSetBody(res, fields, 2, false);

	return res;
}

LLVMTypeRef gen_type(LLVMModuleRef mod, type_t *ty)
{
	if (ty->kind == TY_PLAIN)
		return gen_plain_type(ty->v_plain);
	if (ty->kind == TY_POINTER)
		return LLVMPointerType(gen_type(mod, ty->v_base), 0);
	if (ty->kind == TY_ARRAY)
		return gen_array_type(mod, ty);
	if (type_is_string(ty))
		return gen_str_type(mod);
	if (ty->kind == TY_OBJECT)
		return gen_object_type(mod, ty);
	return LLVMVoidType();
}

LLVMTypeRef gen_function_type(LLVMModuleRef mod, fn_expr_t *func)
{
	LLVMTypeRef *param_types = NULL;
	LLVMTypeRef func_type;

	if (func->n_params) {
		param_types = slab_alloc(func->n_params * sizeof(LLVMTypeRef));

		for (int i = 0; i < func->n_params; i++)
			param_types[i] = gen_type(mod, func->params[i]->type);
	}

	func_type = LLVMFunctionType(gen_type(mod, func->return_type),
				     param_types, func->n_params, 0);
	return func_type;
}

static LLVMValueRef gen_local_value(LLVMBuilderRef builder,
				    fn_context_t *context, const char *name)
{
	LLVMValueRef val;
	LLVMValueRef tmp;
	LLVMTypeRef o_type;
	var_decl_expr_t *decl;

	decl = node_resolve_local(context->current_block, name, 0);
	val = fn_find_local(context, name);

	if (!decl)
		error("emit[no-local]: no ref to local");

	if (decl->type->kind == TY_OBJECT) {
		o_type = gen_type(context->llvm_mod, decl->type);

		char *alloca_name;
		if (context->settings->emit_varnames) {
			alloca_name = slab_alloc(128);
			snprintf(alloca_name, 128, "copy.%s", name);
		} else {
			alloca_name = slab_strdup("");
		}

		tmp = LLVMBuildAlloca(builder, o_type, alloca_name);

		LLVMBuildStore(
		    builder,
		    gen_zero_value_for(builder, context->llvm_mod, decl->type),
		    tmp);

		emit_copy(builder, context, tmp, val, decl->type);

		return LLVMBuildLoad2(
		    builder, gen_type(context->llvm_mod, decl->type), tmp, "");
	}

	if (LLVMIsAAllocaInst(val)) {
		val =
		    LLVMBuildLoad2(builder, LLVMGetAllocatedType(val), val, "");
	}

	return val;
}

static LLVMValueRef gen_member_value(LLVMBuilderRef builder,
				     fn_context_t *context, value_expr_t *node)
{
	LLVMValueRef ptr;
	LLVMValueRef ret;
	LLVMTypeRef member_type;
	type_t *local_type;
	type_t *field_type;

	ptr = gen_ptr_to_member(builder, context, node);
	local_type =
	    node_resolve_local(context->current_block, node->name, 0)->type;

	if (local_type->kind == TY_POINTER)
		local_type = local_type->v_base;

	field_type = type_object_field_type(local_type->v_object, node->member);
	member_type = gen_type(context->llvm_mod, field_type);

	if (field_type->kind == TY_OBJECT) {
		LLVMValueRef alloca = LLVMBuildAlloca(builder, member_type, "");
		LLVMBuildStore(
		    builder,
		    gen_zero_value_for(builder, context->llvm_mod, field_type),
		    alloca);

		emit_copy(builder, context, alloca, ptr, field_type);
		ptr = alloca;
	}

	ret = LLVMBuildLoad2(builder, member_type, ptr, "");

	return ret;
}

static LLVMValueRef gen_addr(fn_context_t *context, const char *name)
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
	LLVMValueRef val;
	LLVMValueRef ret;

	/* If local is a pointer, we need to dereference it twice, because we
	   alloca the pointer too. */
	var_decl_expr_t *var =
	    node_resolve_local(context->current_block, name, 0);
	if (var->type->kind == TY_POINTER) {
		val = LLVMBuildLoad2(
		    builder, gen_type(context->llvm_mod, var->type), local, "");

		if (LLVMIsAAllocaInst(val))
			ret = LLVMBuildLoad2(builder, deref_to, val, "");
		else
			ret = val;

		return ret;
	}

	return LLVMBuildLoad2(builder, deref_to, local, "");
}

static LLVMTypeRef llvm_type_from_plain(plain_type kind)
{
	switch (kind) {
	case PT_BOOL:
		return LLVMIntType(1);
	case PT_I8:
		return LLVMIntType(8);
	case PT_I16:
		return LLVMIntType(16);
	case PT_I32:
		return LLVMIntType(32);
	case PT_I64:
	default:
		return LLVMIntType(64);
	}
}

static LLVMValueRef gen_literal_value(LLVMBuilderRef builder,
				      fn_context_t *context,
				      literal_expr_t *lit)
{
	if (lit->type->kind == TY_PLAIN)
		return LLVMConstInt(llvm_type_from_plain(lit->type->v_plain),
				    lit->v_int, false);

	/* Return a str struct. */
	if (type_is_string(lit->type)) {
		LLVMValueRef values[3];

		values[0] =
		    LLVMConstInt(LLVMInt64Type(), lit->v_str.len, false);
		values[1] =
		    LLVMBuildGlobalStringPtr(builder, lit->v_str.ptr, "");
		values[2] = LLVMConstInt(LLVMInt32Type(), 0, false);

		return LLVMConstNamedStruct(gen_str_type(context->llvm_mod),
					    values, 3);
	}

	error("emit: could not generate literal");
	return NULL;
}

static LLVMValueRef gen_cmp(LLVMBuilderRef builder, fn_context_t *context,
			    value_expr_t *value, LLVMIntPredicate op)
{
	LLVMValueRef left, right;

	/* Integer comparison. */
	if (value->left->return_type->kind != TY_PLAIN
	    || value->right->return_type->kind != TY_PLAIN) {
		error("emit[cmp]: cannot emit comparison for %s == %s",
		      type_name(value->left->return_type),
		      type_name(value->right->return_type));
	}

	left = gen_new_value(builder, context, value->left);
	right = gen_new_value(builder, context, value->right);

	/* If we're comparing 2 different types, zero-extend the smaller
	   one to make them comparable. */
	if (!type_cmp(value->left->return_type, value->right->return_type)) {
		if (type_sizeof(value->left->return_type)
		    > type_sizeof(value->right->return_type)) {

			/* zext right -> left */
			right =
			    LLVMBuildZExt(builder, right, LLVMTypeOf(left), "");

		} else {

			/* zext left -> right */
			left =
			    LLVMBuildZExt(builder, left, LLVMTypeOf(right), "");
		}
	}

	return LLVMBuildICmp(builder, op, left, right, "");
}

static LLVMValueRef gen_const_array(LLVMBuilderRef builder,
				    fn_context_t *context, value_expr_t *value)
{
	tuple_expr_t *tuple;
	LLVMValueRef array;
	LLVMValueRef fields[2];
	LLVMValueRef *zero_values;
	LLVMTypeRef ty;
	LLVMTypeRef elem_ty;

	if (value->type != VE_TUPLE)
		error("emit: tried to gen const array from non-tuple value");

	tuple = value->tuple;
	ty = gen_array_type(context->llvm_mod, value->return_type);
	elem_ty = gen_type(context->llvm_mod, value->return_type->v_base);

	/* Create a global which will store our data. */
	array = LLVMAddGlobal(context->llvm_mod,
			      LLVMArrayType(elem_ty, tuple->len), "");

	/* Initialize it all to 0. */
	zero_values = slab_alloc_array(tuple->len, sizeof(LLVMValueRef));
	for (int i = 0; i < tuple->len; i++) {
		zero_values[i] = gen_zero_value_for(builder, context->llvm_mod,
						    tuple->element_type);
	}

	LLVMSetInitializer(array,
			   LLVMConstArray(elem_ty, zero_values, tuple->len));
	LLVMSetLinkage(array, LLVMPrivateLinkage);

	/* Insert the elements into said global array. */
	for (int i = 0; i < tuple->len; i++) {
		LLVMValueRef indices[2];
		LLVMValueRef ptr;

		indices[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
		indices[1] = LLVMConstInt(LLVMInt32Type(), i, false);

		ptr = LLVMBuildGEP2(builder, LLVMArrayType(elem_ty, tuple->len),
				    array, indices, 2, "");
		LLVMBuildStore(
		    builder, gen_new_value(builder, context, tuple->values[i]),
		    ptr);
	}

	fields[0] = array;
	fields[1] = LLVMConstInt(LLVMInt64Type(), tuple->len, false);

	return LLVMConstNamedStruct(ty, fields, 2);
}

static LLVMValueRef gen_cast(LLVMBuilderRef builder, fn_context_t *context,
			     value_expr_t *value)
{
	typedef LLVMValueRef (*llvm_cast_func)(LLVMBuilderRef, LLVMValueRef,
					       LLVMTypeRef, const char *);

	LLVMValueRef value_to_cast;
	llvm_cast_func casting_func;

	value_to_cast = gen_new_value(builder, context, value->cast_value);

	if (type_is_integer(value->return_type)
	    && type_is_integer(value->cast_value->return_type)) {
		casting_func = LLVMBuildIntCast;
	}

	if (value->return_type->kind == TY_POINTER) {
		if (type_is_integer(value->cast_value->return_type))
			casting_func = LLVMBuildIntToPtr;
		if (value->cast_value->return_type->kind == TY_POINTER)
			casting_func = LLVMBuildPointerCast;
	}

	if (!casting_func)
		error("emit: cannot select casting_func for %s -> %s",
		      type_name(value->cast_value->return_type),
		      type_name(value->return_type));

	return casting_func(builder, value_to_cast,
			    gen_type(context->llvm_mod, value->return_type),
			    "");
}

/**
 * Generate a new value in the block from the value expression given. It can be
 * a reference to a variable, a literal value, a call or two-hand-side
 * operation.
 */
static LLVMValueRef gen_new_value(LLVMBuilderRef builder, fn_context_t *context,
				  value_expr_t *value)
{
	switch (value->type) {
	case VE_NULL:
		error("emit: tried to generate a null value");
		break;
	case VE_REF:
		return gen_local_value(builder, context, value->name);
	case VE_MREF:
		return gen_member_value(builder, context, value);
	case VE_LIT:
		return gen_literal_value(builder, context, value->literal);
	case VE_CALL:
		return emit_call_node(builder, context, value->call);
	case VE_PTR:
		return gen_addr(context, value->name);
	case VE_MPTR:
		return gen_ptr_to_member(builder, context, value);
	case VE_DEREF:
		return gen_deref(
		    builder, context, value->name,
		    gen_type(context->llvm_mod, value->return_type));
	case VE_EQ:
		return gen_cmp(builder, context, value, LLVMIntEQ);
	case VE_NEQ:
		return gen_cmp(builder, context, value, LLVMIntNE);
	case VE_TUPLE:
		return gen_const_array(builder, context, value);
	case VE_CAST:
		return gen_cast(builder, context, value);
	default:
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
			error("emit: unknown operation: %s",
			      value_expr_type_name(value->type));
		}

		if (!new)
			error("emit: failed to build math operation");

		return new;
	}
}

static LLVMValueRef fn_find_local(fn_context_t *context, const char *name)
{
	var_decl_expr_t *local;

	for (int i = 0; i < context->n_locals; i++) {
		if (!strcmp(context->local_names[i], name)) {
			local =
			    node_resolve_local(context->current_block, name, 0);
			if (!local)
				goto end;

			local->used_by_emit = true;
			return context->locals[i];
		}
	}

end:
	error("emit[no-local]: could not find local `%s` in `%s`", name,
	      E_AS_FN(context->func->data)->name);
}

void emit_function_decl(LLVMModuleRef mod, fn_expr_t *fn, LLVMLinkage linkage)
{
	LLVMValueRef f;
	char *ident;

	ident = fn->flags & FN_NOMANGLE ? fn->name : xc_mangle(fn);
	f = LLVMAddFunction(mod, ident, gen_function_type(mod, fn));
	LLVMSetLinkage(f, linkage);
}

static void emit_copy(LLVMBuilderRef builder, fn_context_t *context,
		      LLVMValueRef to, LLVMValueRef from, type_t *ty)
{
	LLVMValueRef func;
	LLVMValueRef args[2];
	fn_candidates_t *results;
	fn_expr_t *match, *f;
	char *symbol;

	results = module_find_fn_candidates(context->module, "copy");
	if (!results->n_candidates) {
		error("emit[no-copy]: missing `fn copy(&%s, &%s)`",
		      type_name(ty), type_name(ty));
	}

	/* match a copy<T>(&T, &T) */

	match = NULL;
	for (int i = 0; i < results->n_candidates; i++) {
		f = results->candidate[i];

		if (f->n_params != 2)
			continue;
		if (f->params[0]->type->kind != TY_POINTER)
			continue;
		if (!type_cmp(f->params[0]->type, f->params[1]->type))
			continue;
		if (!type_cmp(ty, f->params[0]->type->v_base))
			continue;

		match = f;
		break;
	}

	if (!match) {
		error("emit[no-copy]: missing `fn copy(&%s, &%s)`",
		      type_name(ty), type_name(ty));
	}

	symbol = xc_mangle(match);
	func = LLVMGetNamedFunction(context->llvm_mod, symbol);

	args[0] = to;
	args[1] = from;

	LLVMBuildCall2(builder, gen_function_type(context->llvm_mod, match),
		       func, args, 2, "");
}

typedef struct
{
	LLVMValueRef func;
	LLVMTypeRef type;
} gen_drop_res_t;

static gen_drop_res_t gen_drop_for(fn_context_t *context, type_t *ty)
{
	gen_drop_res_t res;
	LLVMBuilderRef builder;
	LLVMBasicBlockRef code;
	LLVMValueRef tmp;
	LLVMValueRef indices[2];
	char *name;
	fn_expr_t *decl;

	decl = module_add_decl(context->module);

	decl->name = "drop";
	decl->n_params = 1;
	decl->params = realloc_ptr_array(decl->params, 1);
	decl->params[0] = slab_alloc(sizeof(var_decl_expr_t));

	decl->params[0]->name = "_";
	decl->params[0]->type = type_copy(type_pointer_of(ty));

	name = xc_mangle(decl);

	res.type = gen_type(context->llvm_mod, type_pointer_of(ty));
	res.type = LLVMFunctionType(LLVMVoidType(), &res.type, 1, false);
	res.func = LLVMAddFunction(context->llvm_mod, name, res.type);

	LLVMSetLinkage(res.func, LLVMInternalLinkage);

	builder = LLVMCreateBuilder();
	code = LLVMAppendBasicBlock(res.func, "start");
	LLVMPositionBuilderAtEnd(builder, code);

	if (context->settings->emit_stacktrace) {
		emit_stackpush(builder, context->llvm_mod, "drop",
			       "<generated>");
	}

	for (int i = 0; i < ty->v_object->n_fields; i++) {
		if (ty->v_object->fields[i]->kind != TY_OBJECT)
			continue;

		indices[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
		indices[1] = LLVMConstInt(LLVMInt32Type(), i, false);

		tmp = LLVMBuildGEP2(builder, gen_type(context->llvm_mod, ty),
				    LLVMGetParam(res.func, 0), indices, 2, "");

		auto_drop_rule_t rule;
		rule.type = ty->v_object->fields[i];
		rule.value = tmp;

		/* Generate a drop for an object. */
		emit_drop(builder, context, &rule);
	}

	if (context->settings->emit_stacktrace)
		emit_stackpop(builder, context->llvm_mod);

	LLVMBuildRetVoid(builder);
	LLVMDisposeBuilder(builder);

	return res;
}

static void emit_drop(LLVMBuilderRef builder, fn_context_t *context,
		      auto_drop_rule_t *rule)
{
	gen_drop_res_t res = {0};
	fn_candidates_t *results;
	fn_expr_t *match, *f;
	char *symbol;

	match = NULL;
	results = module_find_fn_candidates(context->module, "drop");

	/* match a drop(&T) */

	for (int i = 0; i < results->n_candidates; i++) {
		f = results->candidate[i];

		if (f->n_params != 1)
			continue;
		if (f->params[0]->type->kind != TY_POINTER)
			continue;
		if (!type_cmp(rule->type, f->params[0]->type->v_base))
			continue;

		match = f;
		break;
	}

	if (!match) {
		res = gen_drop_for(context, rule->type);
	} else {
		symbol = xc_mangle(match);
		res.func = LLVMGetNamedFunction(context->llvm_mod, symbol);
		res.type = gen_function_type(context->llvm_mod, match);
	}

	if (!res.func) {
		error("emit[no-drop]: missing `fn drop(&%s)`",
		      type_name(rule->type));
	}

	LLVMBuildCall2(builder, res.type, res.func, &rule->value, 1, "");
}

void emit_return_node(LLVMBuilderRef builder, fn_context_t *context,
		      expr_t *node)
{
	LLVMValueRef ret;
	value_expr_t *value;

	value = node->data;

	if (value->type != VE_NULL) {
		ret = gen_new_value(builder, context, value);
		if (!ret) {
			error("emit: could not generate return value for %s",
			      E_AS_FN(context->func->data)->name);
		}

		LLVMBuildStore(builder, ret, context->ret_value);
	}

	LLVMBuildBr(builder, context->end);
}

static LLVMValueRef gen_ptr_to_member(LLVMBuilderRef builder,
				      fn_context_t *context, value_expr_t *node)
{
	LLVMValueRef indices[2];
	LLVMValueRef ptr;
	LLVMValueRef obj;
	var_decl_expr_t *local;
	type_t *local_type;

	if (node->type != VE_MREF && node->type != VE_MPTR)
		error("emit: trying to getelementptr into non-object ref");

	local = node_resolve_local(context->current_block, node->name, 0);
	local_type = local->type;
	if (local_type->kind == TY_POINTER)
		local_type = local_type->v_base;

	if (local_type->kind != TY_OBJECT)
		error("emit: non-object type for MREF");

	indices[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
	indices[1] = LLVMConstInt(
	    LLVMInt32Type(),
	    type_object_field_index(local_type->v_object, node->member), false);

	obj = fn_find_local(context, node->name);
	ptr = LLVMBuildGEP2(builder, gen_type(context->llvm_mod, local_type),
			    obj, indices, 2, "");

	return ptr;
}

static void emit_assign_node(LLVMBuilderRef builder, fn_context_t *context,
			     expr_t *node)
{
	LLVMValueRef to;
	LLVMValueRef value;
	assign_expr_t *data;

	data = node->data;

	/* Check for matching types on left & right side. */
	if (!type_cmp(data->to->return_type, data->value->return_type)) {
		error("emit[type-mismatch]: mismatched types on left and right "
		      "side of assignment to %s",
		      data->to->name);
	}

	if (data->to->type == VE_DEREF) {

		/* *var = xxx */
		LLVMValueRef local;
		LLVMValueRef ptr;

		type_t *ptr_type = type_pointer_of(data->to->return_type);

		local = fn_find_local(context, data->to->name);
		if (LLVMIsAAllocaInst(local)) {
			ptr = LLVMBuildLoad2(
			    builder, gen_type(context->llvm_mod, ptr_type),
			    gen_addr(context, data->to->name), "");
		} else {
			ptr = local;
		}

		to = ptr;

	} else if (data->to->type == VE_REF) {

		/* var = xxx */
		to = gen_addr(context, data->to->name);

	} else if (data->to->type == VE_MREF) {

		/* var.field = xxx */
		to = gen_ptr_to_member(builder, context, data->to);

	} else {
		error("emit[assign-dest]: cannot emit assign to %s",
		      value_expr_type_name(data->to->type));
	}

	/* Call copy for assignment. */
	if (data->to->return_type->kind == TY_OBJECT) {
		LLVMValueRef tmp;
		LLVMValueRef alloca;

		tmp = gen_new_value(builder, context, data->value);

		if (!LLVMIsAAllocaInst(tmp)) {
			alloca = LLVMBuildAlloca(builder, LLVMTypeOf(tmp), "");
			LLVMBuildStore(builder, tmp, alloca);
			emit_copy(builder, context, to, alloca,
				  data->to->return_type);
		} else {
			emit_copy(builder, context, to, tmp,
				  data->to->return_type);
		}

		return;
	}

	/* Make a temporary for the object we want to store. */
	value = gen_new_value(builder, context, data->value);
	LLVMBuildStore(builder, value, to);
}

static LLVMValueRef emit_call_node(LLVMBuilderRef builder,
				   fn_context_t *context, call_expr_t *call)
{
	LLVMValueRef *args;
	LLVMValueRef func;
	LLVMValueRef ret;
	char *name;
	int n_args;

	args = slab_alloc(call->n_args * sizeof(LLVMValueRef));
	n_args = call->n_args;

	for (int i = 0; i < n_args; i++)
		args[i] = gen_new_value(builder, context, call->args[i]);

	name = call->func->name;
	if (!(call->func->flags & FN_NOMANGLE))
		name = xc_mangle(call->func);

	func = LLVMGetNamedFunction(context->llvm_mod, name);
	if (!func)
		error("emit[no-func]: missing named func %s", name);

	ret = LLVMBuildCall2(builder,
			     gen_function_type(context->llvm_mod, call->func),
			     func, args, n_args, "");

	return ret;
}

static LLVMValueRef gen_zero_value_for(LLVMBuilderRef builder,
				       LLVMModuleRef mod, type_t *ty)
{
	if (ty->kind == TY_PLAIN) {
		return LLVMConstInt(gen_type(mod, ty), 0, false);
	} else if (ty->kind == TY_OBJECT) {
		object_type_t *o = ty->v_object;
		LLVMValueRef object;
		LLVMValueRef *init_values =
		    slab_alloc(o->n_fields * sizeof(LLVMValueRef));

		for (int i = 0; i < o->n_fields; i++) {
			init_values[i] =
			    gen_zero_value_for(builder, mod, o->fields[i]);
		}

		object = LLVMConstStruct(init_values, o->n_fields, false);
		return object;
	} else if (ty->kind == TY_POINTER) {
		return LLVMConstPointerNull(gen_type(mod, ty));
	} else {
		error("emit: no idea how to emit default value for %s",
		      type_name(ty));
	}
}

void emit_var_decl(LLVMBuilderRef builder, fn_context_t *context, expr_t *node)
{
	LLVMValueRef alloca;
	var_decl_expr_t *decl;
	char *varname;

	decl = node->data;
	varname = slab_alloc(strlen(decl->name) + 7);
	strcpy(varname, "local.");
	strcat(varname, decl->name);

	alloca =
	    LLVMBuildAlloca(builder, gen_type(context->llvm_mod, decl->type),
			    context->settings->emit_varnames ? varname : "");

	/* If it's an object, zero-initialize it & add any automatic drops. */
	if (decl->type->kind == TY_OBJECT) {
		LLVMBuildStore(
		    builder,
		    gen_zero_value_for(builder, context->llvm_mod, decl->type),
		    alloca);

		auto_drop_rule_t *drop = slab_alloc(sizeof(*drop));
		drop->type = type_copy(decl->type);
		drop->value = alloca;

		fn_add_auto_drop(context, drop);
	}

	fn_context_add_local(context, alloca, decl->name);
}

static void emit_conditional_block(LLVMBuilderRef builder,
				   fn_context_t *context, expr_t *node,
				   expr_t *block, bool negate)
{
	condition_expr_t *cond;
	LLVMBasicBlockRef cond_block;
	LLVMBasicBlockRef pass_block;
	LLVMValueRef cond_value;
	expr_t *walker;
	expr_t *previous_block;

	cond = node->data;
	cond_value = gen_new_value(builder, context, cond->cond);
	cond_block = LLVMInsertBasicBlock(context->end, "cond");
	pass_block = LLVMInsertBasicBlock(context->end, "pass");
	previous_block = context->current_block;

	if (negate)
		LLVMBuildCondBr(builder, cond_value, pass_block, cond_block);
	else
		LLVMBuildCondBr(builder, cond_value, cond_block, pass_block);

	LLVMPositionBuilderAtEnd(builder, cond_block);

	walker = block->child;
	context->current_block = block;

	while (walker) {
		emit_node(builder, context, walker);
		walker = walker->next;
	}

	LLVMBuildBr(builder, pass_block);

	LLVMPositionBuilderAtEnd(builder, pass_block);
	context->current_block = previous_block;
}

static void emit_condition_node(LLVMBuilderRef builder, fn_context_t *context,
				expr_t *node)
{
	condition_expr_t *cond;
	LLVMValueRef cond_value;
	LLVMBasicBlockRef if_block;
	LLVMBasicBlockRef else_block;
	LLVMBasicBlockRef pass_block;
	expr_t *walker;
	expr_t *previous_block;

	cond = node->data;
	previous_block = context->current_block;

	/* Only if the `if` block is non-empty */
	if (cond->else_block && cond->if_block->type != E_SKIP
	    && cond->else_block->type == E_SKIP) {
		emit_conditional_block(builder, context, node, cond->if_block,
				       false);
		return;
	}

	/* Only if the `else` block is non-empty */
	if (cond->else_block && cond->else_block->type != E_SKIP
	    && cond->if_block->type == E_SKIP) {
		emit_conditional_block(builder, context, node, cond->else_block,
				       true);
		return;
	}

	/* If both are empty, remove the whole condition. */
	if (cond->if_block && cond->if_block->type == E_SKIP
	    && (cond->else_block && cond->else_block->type == E_SKIP)) {
		return;
	}

	/* Prepare the basic blocks. */
	cond_value = gen_new_value(builder, context, cond->cond);
	if_block = LLVMInsertBasicBlock(context->end, "");

	else_block = NULL;
	if (cond->else_block)
		else_block = LLVMInsertBasicBlock(context->end, "");

	pass_block = LLVMInsertBasicBlock(context->end, "");

	LLVMBuildCondBr(builder, cond_value, if_block,
			cond->else_block ? else_block : pass_block);

	/* if {} */
	LLVMPositionBuilderAtEnd(builder, if_block);

	walker = cond->if_block->child;
	context->current_block = cond->if_block;
	while (walker) {
		emit_node(builder, context, walker);
		walker = walker->next;
	}

	LLVMBuildBr(builder, pass_block);

	/* else {} */
	if (cond->else_block && cond->else_block->type != E_SKIP) {
		LLVMPositionBuilderAtEnd(builder, else_block);

		walker = cond->else_block->child;
		context->current_block = cond->else_block;
		while (walker) {
			emit_node(builder, context, walker);
			walker = walker->next;
		}

		LLVMBuildBr(builder, pass_block);
	}

	/* go back */
	LLVMPositionBuilderAtEnd(builder, pass_block);
	context->current_block = previous_block;
}

void emit_node(LLVMBuilderRef builder, fn_context_t *context, expr_t *node)
{
	if (!node)
		return;

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
	case E_CONDITION:
		emit_condition_node(builder, context, node);
	case E_SKIP:
		break;
	default:
		error("emit[node-emit]: undefined emit semantics for node");
	}
}

void emit_stackpush(LLVMBuilderRef builder, LLVMModuleRef mod,
		    const char *symbol, const char *file)
{
	LLVMTypeRef param_types[2];
	LLVMValueRef args[2];

	param_types[0] = LLVMPointerType(LLVMInt8Type(), 0);
	param_types[1] = LLVMPointerType(LLVMInt8Type(), 0);
	args[0] = LLVMBuildGlobalString(builder, symbol, "");
	args[1] = LLVMBuildGlobalString(builder, file, "");

	// TODO: do some string interning for stackpush calls

	LLVMBuildCall2(
	    builder, LLVMFunctionType(LLVMVoidType(), param_types, 2, false),
	    LLVMGetNamedFunction(mod, "__mocha_stackpush"), args, 2, "");
}

void emit_stackpop(LLVMBuilderRef builder, LLVMModuleRef mod)
{
	LLVMBuildCall2(
	    builder, LLVMFunctionType(LLVMVoidType(), NULL, 0, false),
	    LLVMGetNamedFunction(mod, "__mocha_stackpop"), NULL, 0, "");
}

void emit_function_body(settings_t *settings, LLVMModuleRef mod, expr_t *module,
			expr_t *fn)
{
	LLVMValueRef func;
	fn_context_t context;
	char *name;

	name = xc_mangle(fn->data);
	func = LLVMGetNamedFunction(mod, name);

	memset(&context, 0, sizeof(context));
	context.func = fn;
	context.module = module;
	context.llvm_func = func;
	context.llvm_mod = mod;
	context.settings = settings;
	context.current_block = fn;

	LLVMBasicBlockRef args_block, code_block, end_block;
	LLVMBuilderRef builder;
	expr_t *walker;

	args_block = LLVMAppendBasicBlock(func, "args");
	builder = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(builder, args_block);

	/* We want to move all copied arguments into alloca values.  */
	fn_expr_t *data = fn->data;
	for (int i = 0; i < data->n_params; i++) {
		LLVMValueRef arg;

		if (data->params[i]->type->kind == TY_POINTER) {
			arg = LLVMGetParam(func, i);
		} else {
			char *varname =
			    slab_alloc(strlen(data->params[i]->name) + 7);
			strcpy(varname, "local.");
			strcat(varname, data->params[i]->name);

			arg = LLVMBuildAlloca(
			    builder, gen_type(mod, data->params[i]->type),
			    settings->emit_varnames ? varname : "");
			LLVMBuildStore(builder, LLVMGetParam(func, i), arg);
		}

		fn_context_add_local(&context, arg, data->params[i]->name);

		/* If a parameter is a copied object, drop it. */
		if (data->params[i]->type->kind == TY_OBJECT) {
			auto_drop_rule_t *drop = slab_alloc(sizeof(*drop));

			drop->value = arg;
			drop->type = type_copy(data->params[i]->type);

			fn_add_auto_drop(&context, drop);
		}
	}

	/* If we have a return value, store it into a local variable. */
	if (data->return_type->kind != TY_NULL) {
		context.ret_value =
		    LLVMBuildAlloca(builder, gen_type(mod, data->return_type),
				    settings->emit_varnames ? "ret" : "");
	}

	/* args -> start */
	code_block = LLVMAppendBasicBlock(func, "start");
	LLVMBuildBr(builder, code_block);

	/* end block with return statement */
	end_block = LLVMAppendBasicBlock(func, "end");
	context.end = end_block;

	/* Code block */
	LLVMPositionBuilderAtEnd(builder, code_block);

	if (settings->emit_stacktrace) {
		emit_stackpush(builder, mod, E_AS_FN(fn->data)->name,
			       E_AS_MOD(module->data)->source_name);
	}

	walker = fn->child;
	while (walker) {
		emit_node(builder, &context, walker);
		walker = walker->next;
	}

	LLVMPositionBuilderAtEnd(builder, end_block);

	/* Generate drop calls for allocated resources */
	for (int i = 0; i < context.n_auto_drops; i++)
		emit_drop(builder, &context, context.auto_drops[i]);

	/* Stack pop */
	if (settings->emit_stacktrace)
		emit_stackpop(builder, context.llvm_mod);

	/* Return from function */
	if (data->return_type->kind == TY_NULL) {
		LLVMBuildRetVoid(builder);
	} else {
		LLVMValueRef ret_value =
		    LLVMBuildLoad2(builder, gen_type(mod, data->return_type),
				   context.ret_value, "");
		LLVMBuildRet(builder, ret_value);
	}

	if (LLVMVerifyFunction(func, LLVMPrintMessageAction)) {
		error("emit[llvm-ir]: something is wrong with the emitted `%s` "
		      "function",
		      name);
	}

	LLVMDisposeBuilder(builder);
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

	LLVMValueRef main_func = LLVMGetNamedFunction(mod, "_M4mainv");
	if (!main_func)
		error("emit[no-main]: missing main function in module");

	LLVMBuildCall2(builder, LLVMFunctionType(LLVMVoidType(), NULL, 0, 0),
		       main_func, NULL, 0, "");

	return_value = LLVMConstNull(LLVMInt32Type());
	LLVMBuildRet(builder, return_value);

	LLVMDisposeBuilder(builder);
}

static LLVMModuleRef emit_module_contents(settings_t *settings,
					  LLVMModuleRef mod, expr_t *module)
{
	mod_expr_t *mod_data;
	char *err_msg = NULL;
	expr_t *walker;

	mod_data = E_AS_MOD(module->data);

	/* Declare extern functions. */
	for (int i = 0; i < mod_data->n_decls; i++) {
		emit_function_decl(mod, mod_data->decls[i],
				   LLVMExternalLinkage);
	}

	/* Declare struct types. */
	for (int i = 0; i < mod_data->n_type_decls; i++) {
		if (mod_data->type_decls[i]->kind != TY_OBJECT)
			continue;

		object_type_t *o = mod_data->type_decls[i]->v_object;
		LLVMTypeRef o_type;
		LLVMTypeRef *fields =
		    slab_alloc(o->n_fields * sizeof(LLVMTypeRef));

		o_type =
		    LLVMStructCreateNamed(LLVMGetModuleContext(mod), o->name);

		for (int j = 0; j < o->n_fields; j++)
			fields[j] = gen_type(mod, o->fields[j]);

		LLVMStructSetBody(o_type, fields, o->n_fields, false);

		/* For each struct type, emit all methods declared in it. */
		for (int j = 0; j < o->n_methods; j++) {
			fn_expr_t *func = o->methods[j]->data;

			emit_function_decl(mod, func, LLVMInternalLinkage);
			emit_function_body(settings, mod, module,
					   o->methods[j]);
		}
	}

	/* For emitting functions we need to make 2 passes. The first time we
	   declare all functions as private, and only then during the second
	   pass we may implement the function bodies. */

	walker = module->child;
	while (walker) {
		if (walker->type == E_FUNCTION) {
			emit_function_decl(mod, E_AS_FN(walker->data),
					   LLVMInternalLinkage);
		} else {
			error("emit: cannot emit anything other than a "
			      "function");
		}
		walker = walker->next;
	}

	walker = module->child;
	while (walker) {
		if (walker->type == E_FUNCTION)
			emit_function_body(settings, mod, module, walker);
		walker = walker->next;
	}

	if (LLVMVerifyModule(mod, LLVMPrintMessageAction, &err_msg)) {
		error("emit[llvm-ir]: LLVM failed to generate the `%s` module",
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

void emit_module(settings_t *settings, expr_t *module, const char *out)
{
	LLVMModuleRef mod;
	mod_expr_t *mod_data;

	mod_data = module->data;
	mod = LLVMModuleCreateWithName(mod_data->name);
	LLVMSetSourceFileName(mod, mod_data->source_name,
			      strlen(mod_data->source_name));

	add_builtin_types(mod);

	/* Emit standard modules first */
	for (int i = 0; i < mod_data->std_modules->n_modules; i++) {
		emit_module_contents(settings, mod,
				     mod_data->std_modules->modules[i]);
	}

	/* Then, emit all imported modules. */
	for (int i = 0; i < mod_data->n_imported; i++)
		emit_module_contents(settings, mod, mod_data->imported[i]);

	emit_module_contents(settings, mod, module);

	emit_main_function(mod);

	LLVMPrintModuleToFile(mod, out, NULL);
	LLVMDisposeModule(mod);
}
