#pragma once
#include <nxg/tokenize.h>
#include <nxg/type.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum
{
	N_MODULE,
	N_FUNCTION
} node_t;

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
	char *name;
	arg_t **args;
	size_t n_args;
	var_t **locals;
	size_t n_locals;
	type_t return_type;
} func_t;

typedef struct {
	char *name;
} module_t;

// Free handler
typedef void (*node_free)(void *node);

// Abstract syntax tree
typedef struct node node;
struct node
{
	node_t type;
	node *next;
	node *prev;
	void *data;
	node_free free;
};

void parse(token_list *tokens);
