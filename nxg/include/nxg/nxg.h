/* nxg.h - coffee compiler
   Copyright (c) 2023 mini-rose */

#pragma once
#include <stdbool.h>

#define NXG_MAJOR 0
#define NXG_MINOR 3

#define MAIN_MODULE "__main__"

typedef struct
{
	bool show_ast;
	bool show_tokens;
	bool global;
	bool using_bs;
	char *output;
	char *input;
	char *stdpath;
} settings_t;

#define __unused __attribute__((unused))

/* Compile all input files into a single binary. */
void compile(settings_t *settings);

/* Compile a C source code file into an object. */
char *compile_c_object(char *file);

char *make_modname(char *file);
