#include <libgen.h>
#include <nxg/cc/module.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/nxg.h>
#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void add_module_import(mod_expr_t *module, expr_t *ast)
{
	module->imported = realloc(module->imported,
				   sizeof(expr_t *) * (module->n_imported + 1));
	module->imported[module->n_imported++] = ast;
}

static void add_module_c_object(mod_expr_t *module, char *object)
{
	module->c_objects->objects =
	    realloc(module->c_objects->objects,
		    sizeof(char *) * (module->c_objects->n + 1));
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
		working_dir = strdup(module->source_name);
		if (!strchr(working_dir, '/')) {
			free(working_dir);
			working_dir = strdup(".");
		} else {
			dirname(working_dir);
		}
	} else {
		working_dir = strdup(".");
	}

	snprintf(pathbuf, 512, "%s.ff", file);

	fil = file_new_null(pathbuf);
	if (!fil) {
		free(working_dir);
		return NULL;
	}

	parsed_tokens = tokens(fil);

	modname = make_modname(pathbuf);
	parsed = calloc(1, sizeof(*parsed));
	parsed->data = calloc(1, sizeof(mod_expr_t));

	parsed = parse(module_expr, parsed, settings, parsed_tokens, modname);

	/* Apart from the regular coffee source code, if there is a C file with
	   the same name as the imported module, compile it & link against it
	   in the final stage. */
	snprintf(pathbuf, 512, "%s.c", file);
	if (!access(pathbuf, F_OK)) {
		add_module_c_object(module,
				    compile_c_object(settings, pathbuf));
	}

	free(modname);
	free(working_dir);
	file_destroy(fil);
	token_list_destroy(parsed_tokens);

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
	char *path = calloc(512, 1);
	char *modname = strdup(file + 1);
	mod_expr_t *mod;
	expr_t *imported;
	int n;

	snprintf(path, 512, "%s%s", settings->libpath, file);
	imported = module_import_impl(settings, module, path);

	if (!imported) {
		free(path);
		free(modname);
		return NULL;
	}

	/* Add the module to the module's std. */
	mod_expr_t *parent = module->data;

	parent->std_modules->modules =
	    realloc(parent->std_modules->modules,
		    sizeof(expr_t *) * (parent->std_modules->n_modules + 1));
	parent->std_modules->modules[parent->std_modules->n_modules++] =
	    imported;

	mod = imported->data;

	n = strlen(modname);
	for (int i = 0; i < n; i++) {
		if (modname[i] == '/')
			modname[i] = '.';
	}

	free(mod->name);
	free(path);

	mod->name = modname;
	mod->origin = MO_STD;

	return imported;
}

fn_expr_t *module_add_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;
	fn_expr_t *decl;

	mod->decls =
	    realloc(mod->decls, sizeof(fn_expr_t *) * (mod->n_decls + 1));
	mod->decls[mod->n_decls++] = decl = calloc(1, sizeof(fn_expr_t));

	decl->module = module;

	return decl;
}

fn_expr_t *module_add_local_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;
	fn_expr_t *decl;

	mod->local_decls = realloc(
	    mod->local_decls, sizeof(fn_expr_t *) * (mod->n_local_decls + 1));
	mod->local_decls[mod->n_local_decls++] = decl =
	    calloc(1, sizeof(fn_expr_t));

	decl->module = module;

	return decl;
}

type_t *module_add_type_decl(expr_t *module)
{
	mod_expr_t *mod = module->data;

	mod->type_decls = realloc(mod->type_decls,
				  sizeof(type_t *) * (mod->n_type_decls + 1));
	mod->type_decls[mod->n_type_decls] = type_new();
	return mod->type_decls[mod->n_type_decls++];
}

void add_candidate(fn_candidates_t *resolved, fn_expr_t *ptr)
{
	/* Check if it's already in the list */
	for (int i = 0; i < resolved->n_candidates; i++) {
		if (fn_sigcmp(resolved->candidate[i], ptr))
			return;
	}

	resolved->candidate =
	    realloc(resolved->candidate,
		    sizeof(fn_expr_t *) * (resolved->n_candidates + 1));
	resolved->candidate[resolved->n_candidates++] = ptr;
}

fn_candidates_t *module_find_fn_candidates_impl(expr_t *module, char *name,
						int depth)
{
	mod_expr_t *mod = module->data;
	fn_candidates_t *resolved;
	fn_expr_t *fn;
	expr_t *walker;

	resolved = calloc(1, sizeof(*resolved));

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
			add_candidate(resolved, fn);
	} while ((walker = walker->next));

check_decls:
	/* Local declarations have priority */
	for (int i = 0; i < mod->n_local_decls; i++) {
		fn = mod->local_decls[i];
		if (!strcmp(fn->name, name))
			add_candidate(resolved, fn);
	}

	/* Extern declarations have less priority, so local functions get
	   resolved first */
	for (int i = 0; i < mod->n_decls; i++) {
		fn = mod->decls[i];
		if (!strcmp(fn->name, name))
			add_candidate(resolved, fn);
	}

	/* Check the imported modules */
	for (int i = 0; i < mod->n_imported; i++) {
		fn_candidates_t *imported;

		/* Keep the same depth for imported modules. */
		imported = module_find_fn_candidates_impl(mod->imported[i],
							  name, depth);

		for (int j = 0; j < imported->n_candidates; j++)
			add_candidate(resolved, imported->candidate[j]);

		free(imported->candidate);
		free(imported);
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
				add_candidate(resolved, imported->candidate[j]);

			free(imported->candidate);
			free(imported);
		}
	}

	return resolved;
}

fn_candidates_t *module_find_fn_candidates(expr_t *module, char *name)
{
	return module_find_fn_candidates_impl(module, name, 0);
}
