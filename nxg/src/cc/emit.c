#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
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
static void emit_copy(LLVMBuilderRef builder, fn_context_t *context,
		      LLVMValueRef to, LLVMValueRef from, type_t *ty);
static LLVMValueRef gen_zero_value_for(LLVMBuilderRef builder,
				       LLVMModuleRef mod, type_t *ty);
static LLVMValueRef fn_find_local(fn_context_t *context, const char *name);
void emit_stackpop(LLVMBuilderRef builder, LLVMModuleRef mod);
static LLVMValueRef gen_ptr_to_member(LLVMBuilderRef builder,
				      fn_context_t *context,
				      value_expr_t *node);
static LLVMValueRef gen_new_value(LLVMBuilderRef builder, fn_context_t *context,
				  value_expr_t *value);
void emit_node(LLVMBuilderRef builder, fn_context_t *context, expr_t *node);

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
	for (int i = 0; i < context->n_auto_drops; i++) {
		type_destroy(context->auto_drops[i]->type);
		free(context->auto_drops[i]);
	}

	for (int i = 0; i < context->n_locals; i++)
		free(context->local_names[i]);

	free(context->locals);
	free(context->local_names);
	free(context->auto_drops);
}

static void fn_add_auto_drop(fn_context_t *context, auto_drop_rule_t *rule)
{
	context->auto_drops =
	    realloc(context->auto_drops,
		    sizeof(auto_drop_rule_t *) * (context->n_auto_drops + 1));
	context->auto_drops[context->n_auto_drops++] = rule;
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

LLVMTypeRef gen_type(LLVMModuleRef mod, type_t *ty)
{
	if (ty->kind == TY_PLAIN)
		return gen_plain_type(mod, ty->v_plain);
	if (ty->kind == TY_POINTER)
		return LLVMPointerType(gen_type(mod, ty->v_base), 0);
	if (ty->kind == TY_ARRAY)
		return LLVMArrayType(gen_type(mod, ty->v_base), ty->len);
	if (is_str_type(ty))
		return gen_str_type(mod);
	if (ty->kind == TY_OBJECT)
		return gen_object_type(mod, ty);
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
			alloca_name = calloc(128, 1);
			snprintf(alloca_name, 128, "copy.%s", name);
		} else {
			alloca_name = strdup("");
		}

		tmp = LLVMBuildAlloca(builder, o_type, alloca_name);
		free(alloca_name);

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

	type_destroy(field_type);
	return ret;
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

static LLVMValueRef gen_literal_value(LLVMBuilderRef builder,
				      fn_context_t *context,
				      literal_expr_t *lit)
{
	if (lit->type->v_plain == PT_I32)
		return LLVMConstInt(LLVMInt32Type(), lit->v_i32, false);
	if (lit->type->v_plain == PT_I64)
		return LLVMConstInt(LLVMInt64Type(), lit->v_i64, false);
	if (lit->type->v_plain == PT_BOOL)
		return LLVMConstInt(LLVMInt1Type(), lit->v_bool, false);

	/* Return a str struct. */
	if (is_str_type(lit->type)) {
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
	return LLVMBuildICmp(builder, op,
			     gen_new_value(builder, context, value->left),
			     gen_new_value(builder, context, value->right), "");
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
		return gen_addr(builder, context, value->name);
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

	symbol = nxg_mangle(match);
	func = LLVMGetNamedFunction(context->llvm_mod, symbol);

	args[0] = to;
	args[1] = from;

	LLVMBuildCall2(builder, gen_function_type(context->llvm_mod, match),
		       func, args, 2, "");

	free(results->candidate);
	free(results);
	free(symbol);
}

static void emit_drop(LLVMBuilderRef builder, fn_context_t *context,
		      auto_drop_rule_t *rule)
{
	LLVMValueRef func;
	fn_candidates_t *results;
	fn_expr_t *match, *f;
	char *symbol;

	results = module_find_fn_candidates(context->module, "drop");
	if (!results->n_candidates) {
		error("emit[no-drop]: missing `fn drop(&%s)`",
		      type_name(rule->type));
	}

	/* match a drop<T>(&T) */

	match = NULL;
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
		error("emit[no-drop]: missing `fn drop(&%s)`",
		      type_name(rule->type));
	}

	symbol = nxg_mangle(match);
	func = LLVMGetNamedFunction(context->llvm_mod, symbol);

	LLVMBuildCall2(builder, gen_function_type(context->llvm_mod, match),
		       func, &rule->value, 1, "");

	free(results->candidate);
	free(results);
	free(symbol);
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
	type_t *field_type;

	if (node->type != VE_MREF && node->type != VE_MPTR)
		error("emit: trying to getelementptr into non-object ref");

	local = node_resolve_local(context->current_block, node->name, 0);
	local_type = local->type;
	if (local_type->kind == TY_POINTER)
		local_type = local_type->v_base;

	if (local_type->kind != TY_OBJECT)
		error("emit: non-object type for MREF");

	field_type = type_object_field_type(local_type->v_object, node->member);

	indices[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
	indices[1] = LLVMConstInt(
	    LLVMInt32Type(),
	    type_object_field_index(local_type->v_object, node->member), false);

	obj = fn_find_local(context, node->name);
	ptr = LLVMBuildGEP2(builder, gen_type(context->llvm_mod, local_type),
			    obj, indices, 2, "");

	type_destroy(field_type);

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
			    gen_addr(builder, context, data->to->name), "");
		} else {
			ptr = local;
		}

		to = ptr;
		type_destroy(ptr_type);

	} else if (data->to->type == VE_REF) {

		/* var = xxx */
		to = gen_addr(builder, context, data->to->name);

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

	args = calloc(call->n_args, sizeof(LLVMValueRef));
	n_args = call->n_args;

	for (int i = 0; i < n_args; i++)
		args[i] = gen_new_value(builder, context, call->args[i]);

	name = call->func->name;
	if (!(call->func->flags & FN_NOMANGLE))
		name = nxg_mangle(call->func);

	func = LLVMGetNamedFunction(context->llvm_mod, name);
	if (!func)
		error("emit[no-func]: missing named func %s", name);

	ret = LLVMBuildCall2(builder,
			     gen_function_type(context->llvm_mod, call->func),
			     func, args, n_args, "");

	free(args);

	if (!(call->func->flags & FN_NOMANGLE))
		free(name);

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
		    calloc(o->n_fields, sizeof(LLVMValueRef));

		for (int i = 0; i < o->n_fields; i++) {
			init_values[i] =
			    gen_zero_value_for(builder, mod, o->fields[i]);
		}

		object = LLVMConstStruct(init_values, o->n_fields, false);
		free(init_values);
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
	varname = calloc(strlen(decl->name) + 7, 1);
	strcpy(varname, "local.");
	strcat(varname, decl->name);

	alloca =
	    LLVMBuildAlloca(builder, gen_type(context->llvm_mod, decl->type),
			    context->settings->emit_varnames ? varname : "");

	free(varname);

	/* If it's an object, zero-initialize it & add any automatic drops. */
	if (decl->type->kind == TY_OBJECT) {
		LLVMBuildStore(
		    builder,
		    gen_zero_value_for(builder, context->llvm_mod, decl->type),
		    alloca);

		auto_drop_rule_t *drop = calloc(1, sizeof(*drop));
		drop->type = type_copy(decl->type);
		drop->value = alloca;

		fn_add_auto_drop(context, drop);
	}

	fn_context_add_local(context, alloca, decl->name);
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
	do {
		emit_node(builder, context, walker);
	} while ((walker = walker->next));

	LLVMBuildBr(builder, pass_block);

	/* else {} */
	if (cond->else_block) {
		LLVMPositionBuilderAtEnd(builder, else_block);

		walker = cond->else_block->child;
		context->current_block = cond->else_block;
		do {
			emit_node(builder, context, walker);
		} while ((walker = walker->next));

		LLVMBuildBr(builder, pass_block);
	}

	/* go back */
	LLVMPositionBuilderAtEnd(builder, pass_block);
	context->current_block = previous_block;
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
	args[0] = LLVMBuildGlobalStringPtr(builder, symbol, "");
	args[1] = LLVMBuildGlobalStringPtr(builder, file, "");

	LLVMBuildCall2(
	    builder, LLVMFunctionType(LLVMVoidType(), param_types, 2, false),
	    LLVMGetNamedFunction(mod, "__cf_stackpush"), args, 2, "");
}

void emit_stackpop(LLVMBuilderRef builder, LLVMModuleRef mod)
{
	LLVMBuildCall2(builder,
		       LLVMFunctionType(LLVMVoidType(), NULL, 0, false),
		       LLVMGetNamedFunction(mod, "__cf_stackpop"), NULL, 0, "");
}

void emit_function_body(settings_t *settings, LLVMModuleRef mod, expr_t *module,
			expr_t *fn)
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
			    calloc(strlen(data->params[i]->name) + 7, 1);
			strcpy(varname, "local.");
			strcat(varname, data->params[i]->name);

			arg = LLVMBuildAlloca(
			    builder, gen_type(mod, data->params[i]->type),
			    settings->emit_varnames ? varname : "");
			LLVMBuildStore(builder, LLVMGetParam(func, i), arg);

			free(varname);
		}

		fn_context_add_local(&context, arg, data->params[i]->name);

		/* If a parameter is a copied object, drop it. */
		if (data->params[i]->type->kind == TY_OBJECT) {
			auto_drop_rule_t *drop = calloc(1, sizeof(*drop));

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

#if 0
	if (LLVMVerifyFunction(func, LLVMPrintMessageAction)) {
		error("emit[llvm-ir]: something is wrong with the emitted `%s` function",
		      name);
	}
#endif

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
		       LLVMGetNamedFunction(mod, "_C4mainv"), NULL, 0, "");

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
		emit_function_decl(mod, module->data, mod_data->decls[i],
				   LLVMExternalLinkage);
	}

	/* Declare struct types. */
	for (int i = 0; i < mod_data->n_type_decls; i++) {
		if (mod_data->type_decls[i]->kind != TY_OBJECT)
			continue;

		object_type_t *o = mod_data->type_decls[i]->v_object;
		LLVMTypeRef o_type;
		LLVMTypeRef *fields = calloc(o->n_fields, sizeof(LLVMTypeRef));

		o_type =
		    LLVMStructCreateNamed(LLVMGetModuleContext(mod), o->name);

		for (int j = 0; j < o->n_fields; j++)
			fields[j] = gen_type(mod, o->fields[j]);

		LLVMStructSetBody(o_type, fields, o->n_fields, false);

		free(fields);
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

#if 0
	if (LLVMVerifyModule(mod, LLVMPrintMessageAction, &err_msg)) {
		error("emit[llvm-ir]: LLVM failed to generate the `%s` module",
		      mod_data->name);
	}
#endif

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

void emit_module(settings_t *settings, expr_t *module, const char *out,
		 bool is_main)
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
