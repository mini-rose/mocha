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
	bool global;
	const char *output;
	const char *input;
} settings_t;

/* Compile all input files into a single binary. */
void compile(settings_t *settings);
