/* mx/build.c - build & compile files
   Copyright (c) 2023 mini-rose */

#include <libgen.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <mx/cc/alloc.h>
#include <mx/cc/emit.h>
#include <mx/cc/module.h>
#include <mx/cc/parser.h>
#include <mx/cc/tokenize.h>
#include <mx/mx.h>
#include <mx/utils/error.h>
#include <mx/utils/file.h>
#include <mx/utils/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static void remove_extension(char *file)
{
	char *p;
	if ((p = strrchr(file, '.')))
		*p = 0;
}

char *make_modname(char *file)
{
	char *start, *modname;

	file = slab_strdup(file);
	if (!(start = strrchr(file, '/')))
		start = file;
	else
		start++;

	remove_extension(file);
	modname = slab_strdup(start);

	return modname;
}

static void build_and_link(settings_t *settings, const char *input_,
			   const char *output, c_objects_t *c_objects)
{
	char cmd[4096];
	char *input;
	FILE *proc;

	input = slab_strdup(input_);
	remove_extension(input);

	/* mod.ll -> mod.bc */
	snprintf(cmd, 4096, "opt -O%s %s > %s.bc", settings->opt, input_,
		 input);
	if (settings->verbose)
		puts(cmd);
	pclose(popen(cmd, "r"));

	/* mod.bc -> output */
	snprintf(cmd, 4096, "clang -o %s -Wno-override-module %s.bc ", output,
		 input);

	if (settings->dyn_linker) {
		snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "-Xlinker '-dynamic-linker=%s' ",
			settings->dyn_linker);
	}

	// TODO: make this safe for more arguments

	for (int i = 0; i < c_objects->n; i++) {
		strcat(cmd, c_objects->objects[i]);
		strcat(cmd, " ");
	}

	if (settings->verbose)
		puts(cmd);
	proc = popen(cmd, "r");

	char c;
	if (read(fileno(proc), &c, 1)) {
		fputc(c, stdout);

		char line[128];
		size_t n;

		while ((n = read(fileno(proc), line, 128)))
			write(STDOUT_FILENO, line, n);

		fflush(stdout);
		error("link stage failed");
	}

	pclose(proc);

	if (settings->pm) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		long long diff = ((tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL))
			       - settings->compile_start;
		double seconds = (double) diff / 1000.0;
		printf("\033[32m Finished\033[0m dev [unoptimized + debuginfo] "
		       "target(s) in %.2fs\n",
		       seconds);
	}

	if (settings->pm_run) {
		printf("\033[32m  Running\033[0m `%s`\n", settings->output);
		snprintf(cmd, 1024, "./%s", settings->output);
		system(cmd);
	}
}

char *compile_c_object(settings_t *settings, char *file)
{
	char *output = slab_alloc(512);
	char cmd[512];
	int len;

	mkdir("/tmp/mx/C", 0777);

	len = snprintf(output, 512, "/tmp/mx/C%s.o", file);
	for (int i = 10; i < len; i++) {
		if (output[i] == '/')
			output[i] = '_';
	}

	snprintf(cmd, 512, "/usr/bin/clang -c -O%s -o %s %s", settings->opt,
		 output, file);
	if (settings->verbose)
		puts(cmd);
	pclose(popen(cmd, "r"));

	return output;
}

static void import_builtins(settings_t *settings, expr_t *module)
{
	char *paths[] = {
	    "std/builtin/stack",
	    "std/builtin/cast",
	    "std/builtin/string",
	};

	for (size_t i = 0; i < LEN(paths); i++) {
		if (!module_std_import(settings, module, paths[i])) {
			error("failed to import builtin %s", paths[i]);
		}
	}
}

void compile(settings_t *settings)
{
	char module_path[512];
	file_t *source;
	token_list *list;
	char *module_name;
	expr_t *ast;

	mkdir("/tmp/mx", 0777);

	source = file_new(settings->input);
	list = tokens(source);

	if (settings->show_tokens)
		token_list_print(list);

	module_name = make_modname(settings->input);

	/* Initialize the top-level module */
	ast = slab_alloc(sizeof(*ast));
	ast->data = slab_alloc(sizeof(mod_expr_t));
	E_AS_MOD(ast->data)->c_objects = slab_alloc(sizeof(c_objects_t));
	E_AS_MOD(ast->data)->std_modules = slab_alloc(sizeof(std_modules_t));

	if (settings->f_builtins)
		import_builtins(settings, ast);

	ast = parse(NULL, ast, settings, list, module_name);

	if (settings->show_ast)
		expr_print(ast);

	snprintf(module_path, 512, "/tmp/mx/%s.ll", module_name);
	emit_module(settings, ast, module_path);
	build_and_link(settings, module_path, settings->output,
		       E_AS_MOD(ast->data)->c_objects);
}
