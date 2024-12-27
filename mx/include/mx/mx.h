/* mx.h - mocha compiler
   Copyright (c) 2023 mini-rose */

#pragma once
#include <stdbool.h>
#include <time.h>

#define MX_MAJOR 0
#define MX_MINOR 9

#if !defined MX_TARGET
# define MX_TARGET "unknown"
#endif

#define DEFAULT_OUT  "a.out"
#define DEFAULT_LD   "/lib/ld-linux-x86-64.so.2"
#define DEFAULT_ROOT "/usr/lib/mocha"

#define LD_MUSL "/lib/ld-musl-x86_64.so.1"

#if !defined MX_ROOT
# define MX_ROOT DEFAULT_ROOT
#endif

typedef struct
{
	bool show_ast;
	bool show_tokens;
	bool global;
	bool using_bs;
	bool verbose;

	/* Package manager */
	bool pm;
	bool pm_run;
	const char *pkgname;
	const char *pkgver;
	long long compile_start;

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
