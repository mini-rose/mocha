#pragma once

#include <nxg/type.h>
#include <stddef.h>

// Variable
typedef struct
{
	char *name, *value;
	type_t type;
} var_t;

// Function argument
typedef struct {
	char *name;
	type_t type;
} arg_t;

// Function
typedef struct
{
	arg_t **args;
	size_t n_args;
	var_t **locals;
	size_t n_locals;
} func_t;

// Module
typedef struct {
	func_t **funcs;
} module_t;
