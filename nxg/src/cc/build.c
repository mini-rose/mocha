/* nxg/build.c
   Copyright (c) 2023 mini-rose */

#include "nxg/cc/module.h"

#include <libgen.h>
#include <nxg/cc/emit.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/nxg.h>
#include <nxg/utils/error.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

	file = strdup(file);
	if (!(start = strrchr(file, '/')))
		start = file;
	else
		start++;

	remove_extension(file);
	modname = strdup(start);
	free(file);

	return modname;
}

static void build_and_link(settings_t *settings, const char *input_,
			   const char *output, char **c_objects,
			   int n_c_objects)
{
	char cmd[1024];
	char *input;
	FILE *proc;

	input = strdup(input_);
	remove_extension(input);

	/* mod.ll -> mod.bc */
	snprintf(cmd, 1024, "/usr/bin/opt -O%s %s > %s.bc", settings->opt,
		 input_, input);
	if (settings->verbose)
		puts(cmd);
	pclose(popen(cmd, "r"));

	/* mod.bc -> mod.s */
	snprintf(cmd, 1024, "/usr/bin/llc -o %s.s %s.bc", input, input);
	if (settings->verbose)
		puts(cmd);
	pclose(popen(cmd, "r"));

	/* mod.s -> mod.o */
	snprintf(cmd, 1024, "/usr/bin/as -o %s.o %s.s", input, input);
	if (settings->verbose)
		puts(cmd);
	pclose(popen(cmd, "r"));

	/* mod.o -> output */
	snprintf(cmd, 1024,
		 "/usr/bin/ld -o %s -dynamic-linker %s /lib/crt1.o "
		 "/lib/crti.o %s.o ",
		 output, settings->dyn_linker, input);

	for (int i = 0; i < n_c_objects; i++) {
		strcat(cmd, c_objects[i]);
		strcat(cmd, " ");
	}

	strcat(cmd, "/lib/crtn.o -lc 2>&1");
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
	free(input);
}

char *compile_c_object(settings_t *settings, char *file)
{
	char *output = calloc(512, 1);
	char cmd[512];

	strcpy(output, file);
	strcat(output, ".o");

	snprintf(cmd, 512, "/usr/bin/clang -c -O%s -o %s %s", settings->opt,
		 output, file);
	if (settings->verbose)
		puts(cmd);
	pclose(popen(cmd, "r"));

	return output;
}

static void import_builtins(settings_t *settings, expr_t *module)
{
	module_std_import(settings, module, "/builtin/string");
	module_std_import(settings, module, "/builtin/print");
}

void compile(settings_t *settings)
{
	char module_path[512];
	file_t *source;
	token_list *list;
	char *module_name;
	expr_t *ast;

	if (settings->jit)
		source = file_stdin();
	else
		source = file_new(settings->input);

	list = tokens(source);

	if (settings->show_tokens)
		token_list_print(list);

	module_name = make_modname(settings->input);

	ast = calloc(1, sizeof(*ast));
	ast->data = calloc(1, sizeof(mod_expr_t));
	import_builtins(settings, ast);

	ast = parse(ast, settings, list, module_name);

	mkdir("/tmp/nxg", 0777);

	if (settings->show_ast)
		expr_print(ast);

	snprintf(module_path, 512, "/tmp/nxg/%s.ll", module_name);
	emit_module(ast, module_path, true);

	build_and_link(settings, module_path, settings->output,
		       E_AS_MOD(ast->data)->c_objects,
		       E_AS_MOD(ast->data)->n_c_objects);

	expr_destroy(ast);
	file_destroy(source);
	token_list_destroy(list);
	free(module_name);
}
