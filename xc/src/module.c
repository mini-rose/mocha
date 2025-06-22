/* xc/module.c - module operations
   Copyright (c) 2023 mini-rose */

#include "module.h"

#include "alloc.h"
#include "parser.h"
#include "tokenize.h"
#include "type.h"
#include "utils/error.h"
#include "utils/file.h"
#include "xc.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void add_module_import(mod_expr_t *module, expr_t *ast)
{
	module->imported =
	    realloc_ptr_array(module->imported, module->n_imported + 1);
	module->imported[module->n_imported++] = ast;
}

static void add_module_c_object(mod_expr_t *module, char *object)
{
	module->c_objects->objects = realloc_ptr_array(
	    module->c_objects->objects, module->c_objects->n + 1);
	module->c_objects->objects[module->c_objects->n++] = object;
}

expr_t *module_import_impl(settings_t *settings, expr_t *module_expr,
			   char *file)
{
	mod_expr_t *module = module_expr->data;
	char pathbuf[512];
	char *working_dir;
	token_list *parsed_tokens;
	char *modname;
	expr_t *parsed;
	file_t *fil;

	if (module->source_name) {
		working_dir = slab_strdup(module->source_name);
		if (!strchr(working_dir, '/')) {
			working_dir = slab_strdup(".");
		} else {
			dirname(working_dir);
		}
	} else {
		working_dir = slab_strdup(".");
	}

	snprintf(pathbuf, 512, "%s.ff", file);

	fil = file_new_null(pathbuf);
	if (!fil)
		return NULL;

	parsed_tokens = tokens(fil);

	modname = make_modname(pathbuf);
	parsed = slab_alloc(sizeof(*parsed));
	parsed->data = slab_alloc(sizeof(mod_expr_t));

	parsed = parse(module_expr, parsed, settings, parsed_tokens, modname);

	/* Apart from the regular mocha source code, if there is a C file with
	   the same name as the imported module, compile it & link against it
	   in the final stage. */
	snprintf(pathbuf, 512, "%s.c", file);
	if (!access(pathbuf, F_OK)) {
		add_module_c_object(module,
				    compile_c_object(settings, pathbuf));
	}

	return parsed;
}

expr_t *module_import(settings_t *settings, expr_t *module, char *file)
{
	expr_t *imported;

	imported = module_import_impl(settings, module, file);
	add_module_import(module->data, imported);

	return imported;
}

expr_t *module_std_import(settings_t *settings, expr_t *module, char *file)
{
	char *path = slab_alloc(512);
	char *modname = slab_strdup(file);
	mod_expr_t *mod;
	expr_t *imported;
	int n;

	snprintf(path, 512, "%s/lib%s%s", settings->sysroot,
		 *file == '/' ? "" : "/", file);

	imported = module_import_impl(settings, module, path);

	if (!imported)
		return NULL;

	/* Add the module to the module's std. */
	mod_expr_t *parent = module->data;

	parent->std_modules->modules = realloc_ptr_array(
	    parent->std_modules->modules, parent->std_modules->n_modules + 1);
	parent->std_modules->modules[parent->std_modules->n_modules++] =
	    imported;

	mod = imported->data;

	n = strlen(modname);
	for (int i = 0; i < n; i++) {
		if (modname[i] == '/')
			modname[i] = '.';
	}

	mod->name = modname;
	mod->origin = MO_STD;

	return imported;
}

fn_expr_t *module_add_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;
	fn_expr_t *decl;

	mod->decls = realloc_ptr_array(mod->decls, mod->n_decls + 1);
	mod->decls[mod->n_decls++] = decl = slab_alloc(sizeof(fn_expr_t));

	decl->module = module;

	return decl;
}

fn_expr_t *module_add_local_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;
	fn_expr_t *decl;

	mod->local_decls =
	    realloc_ptr_array(mod->local_decls, mod->n_local_decls + 1);
	mod->local_decls[mod->n_local_decls++] = decl =
	    slab_alloc(sizeof(fn_expr_t));

	decl->module = module;

	return decl;
}

type_t *module_add_type_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;

	mod->type_decls =
	    realloc_ptr_array(mod->type_decls, mod->n_type_decls + 1);
	mod->type_decls[mod->n_type_decls] = type_new();
	return mod->type_decls[mod->n_type_decls++];
}

void fn_add_candidate(fn_candidates_t *resolved, fn_expr_t *ptr)
{
	/* Check if it's already in the list */
	for (int i = 0; i < resolved->n_candidates; i++) {
		if (fn_sigcmp(resolved->candidate[i], ptr))
			return;
	}

	resolved->candidate =
	    realloc_ptr_array(resolved->candidate, resolved->n_candidates + 1);
	resolved->candidate[resolved->n_candidates++] = ptr;
}

fn_candidates_t *module_find_fn_candidates_impl(expr_t *module, char *name,
						int depth)
{
	mod_expr_t *mod = module->data;
	fn_candidates_t *resolved;
	fn_expr_t *fn;
	expr_t *walker;

	resolved = slab_alloc(sizeof(*resolved));

	if (depth > 1)
		return resolved;

	/* Check the module itself */
	walker = module->child;
	if (!walker)
		goto check_decls;

	do {
		if (walker->type != E_FUNCTION)
			continue;
		fn = walker->data;
		if (!strcmp(fn->name, name))
			fn_add_candidate(resolved, fn);
	} while ((walker = walker->next));

check_decls:
	/* Local declarations have priority */
	for (int i = 0; i < mod->n_local_decls; i++) {
		fn = mod->local_decls[i];
		if (!strcmp(fn->name, name))
			fn_add_candidate(resolved, fn);
	}

	/* Extern declarations have less priority, so local functions get
	   resolved first */
	for (int i = 0; i < mod->n_decls; i++) {
		fn = mod->decls[i];
		if (!strcmp(fn->name, name))
			fn_add_candidate(resolved, fn);
	}

	/* Check the imported modules */
	for (int i = 0; i < mod->n_imported; i++) {
		fn_candidates_t *imported;

		/* Keep the same depth for imported modules. */
		imported = module_find_fn_candidates_impl(mod->imported[i],
							  name, depth);

		for (int j = 0; j < imported->n_candidates; j++)
			fn_add_candidate(resolved, imported->candidate[j]);
	}

	if (mod->std_modules) {
		std_modules_t *stds = E_AS_MOD(module->data)->std_modules;
		for (int i = 0; i < stds->n_modules; i++) {

			/* Don't look inside outselfs. */
			if (!strcmp(E_AS_MOD(module->data)->name,
				    E_AS_MOD(stds->modules[i]->data)->name)) {
				continue;
			}

			/* Increase the depth for std modules so they can
			   include themselfs, without making an infinite
			   recursion loop. */
			fn_candidates_t *imported;
			imported = module_find_fn_candidates_impl(
			    stds->modules[i], name, depth + 1);

			for (int j = 0; j < imported->n_candidates; j++)
				fn_add_candidate(resolved,
						 imported->candidate[j]);
		}
	}

	return resolved;
}

fn_candidates_t *module_find_fn_candidates(expr_t *module, char *name)
{
	return module_find_fn_candidates_impl(module, name, 0);
}

type_t *module_find_named_type_impl(expr_t *module, char *name, int level)
{
	mod_expr_t *this = module->data;
	type_t *tmp;

	if (level > 3)
		return NULL;

	for (int i = 0; i < this->n_type_decls; i++) {
		tmp = this->type_decls[i];

		if (tmp->kind == TY_ALIAS) {
			if (!strcmp(tmp->alias, name))
				return type_copy(tmp->v_base);
		}

		/* Other than aliases, only objects have names. */
		if (tmp->kind != TY_OBJECT)
			continue;

		if (!strcmp(tmp->v_object->name, name))
			return type_copy(tmp);
	}

	/* If we don't find anything, try the imported modules. */
	for (int i = 0; i < this->n_imported; i++) {
		tmp = module_find_named_type(this->imported[i], name);
		if (tmp)
			return tmp;
	}

	/* Maybe the standard libraries? */
	for (int i = 0; i < this->std_modules->n_modules; i++) {
		tmp = module_find_named_type_impl(this->std_modules->modules[i],
						  name, level + 1);
		if (tmp)
			return tmp;
	}

	return NULL;
}

type_t *module_find_named_type(expr_t *module, char *name)
{
	return module_find_named_type_impl(module, name, 0);
}
