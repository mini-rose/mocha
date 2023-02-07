/* nxg/build.c
   Copyright (c) 2023 mini-rose */

#include <nxg/cc/emit.h>
#include <nxg/cc/parser.h>
#include <nxg/cc/tokenize.h>
#include <nxg/nxg.h>
#include <nxg/utils/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *file_basename(const char *file)
{
	size_t n;
	char *name;

	n = strchr(file, '.') - file;
	name = malloc(n + 1);
	strncpy(name, file, n);
	name[n] = 0;

	return name;
}

static void build_and_link(const char *input, const char *output)
{
	char cmd[1024];
	char *name;
	FILE *proc;

	name = file_basename(input);

	/* mod.ll -> mod.s */
	snprintf(cmd, 1024, "/usr/bin/llc -O=2 -o %s.s %s", name, input);
	proc = popen(cmd, "r");

	pclose(proc);

	/* mod.s -> mod.o */
	snprintf(cmd, 1024, "/usr/bin/as -o %s.o %s.s", name, name);
	proc = popen(cmd, "r");
	pclose(proc);

	/* mod.o -> output */
	snprintf(cmd, 1024,
		 "/usr/bin/ld -o %s -dynamic-linker /lib/ld-linux-x86-64.so.2 "
		 "/lib/crt1.o /lib/crti.o %s.o /lib/crtn.o -lc",
		 output, name);
	proc = popen(cmd, "r");
	pclose(proc);

	free(name);
}

void compile(settings_t *settings)
{
	char module_path[512];
	file *source = file_new(settings->input);
	token_list *list = tokens(source);
	char *module_name;
	expr_t *ast;

	if (settings->show_tokens)
		token_list_print(list);

	module_name = file_basename(settings->input);
	ast = parse(list, module_name);

	mkdir("/tmp/nxg", 0777);

	if (settings->show_ast)
		expr_print(ast);

	snprintf(module_path, 512, "/tmp/nxg/%s.ll", module_name);
	emit_module(ast, module_path, true);

	build_and_link(module_path, settings->output);

	if (settings->using_bs) {
		free(settings->input);
		free(settings->output);
	}

	expr_destroy(ast);
	file_destroy(source);
	token_list_destroy(list);
	free(module_name);
}
