/* xc.h - mocha compiler
   Copyright (c) 2023 mini-rose */

#pragma once
#include <stdbool.h>

#define XC_MAJOR 0
#define XC_MINOR 9

#if !defined XC_TARGET
# define XC_TARGET "unknown"
#endif

#define DEFAULT_OUT  "a.out"
#define DEFAULT_ROOT "/usr/local/lib/mocha"

#if !defined XC_ROOT
# define XC_ROOT DEFAULT_ROOT
#endif

#if !defined __warn_unused
# define __warn_unused __attribute__((warn_unused_result))
#endif

typedef struct
{
	bool show_ast;
	bool show_tokens;
	bool global;
	bool using_bs;
	bool verbose;

	/* Emit */
	bool emit_stacktrace;
	bool emit_varnames;

	/* Warnings */
	bool warn_unused;
	bool warn_random;
	bool warn_empty_block;
	bool warn_prefer_ref;
	bool warn_self_name;

	/* Extra */
	bool x_sanitize_alloc;
	bool x_dump_alloc;

	/* Features */
	bool f_builtins;

	char *opt;
	char *output;
	char *input;
	char *sysroot; /* /usr/lib/mocha */
	char *dyn_linker;
} settings_t;

/* Compile all input files into a single binary. */
void compile(settings_t *settings);

/* Compile a C source code file into an object. */
char *compile_c_object(settings_t *settings, char *file);

char *make_modname(char *file);
