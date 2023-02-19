/* nxg.h - coffee compiler
   Copyright (c) 2023 mini-rose */

#pragma once
#include <stdbool.h>

#define NXG_MAJOR 0
#define NXG_MINOR 6

#define MAIN_MODULE "__main__"

#define DEFAULT_OUT  "a.out"
#define DEFAULT_LD   "/lib/ld-linux-x86-64.so.2"
#define DEFAULT_ROOT "/usr/lib/coffee"
#define DEFAULT_OPT  2

#define LD_MUSL "/lib/ld-musl-x86_64.so.1"

#if !defined NXG_ROOT
# define NXG_ROOT DEFAULT_ROOT
#endif

typedef struct
{
	bool show_ast;
	bool show_tokens;
	bool global;
	bool using_bs;
	bool jit;
	bool verbose;
	bool emit_stacktrace;
	bool emit_varnames;
	char *opt;
	char *output;
	char *input;
	char *sysroot; /* /usr/lib/coffee */
	char *dyn_linker;
} settings_t;

/* Compile all input files into a single binary. */
void compile(settings_t *settings);

/* Compile a C source code file into an object. */
char *compile_c_object(settings_t *settings, char *file);

char *make_modname(char *file);
