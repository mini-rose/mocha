/* keyword.h - keyword types
   Copyright (c) 2023 mini-rose */

#pragma once

#define bool _Bool

typedef enum
{
	// Function declaration keyword
	K_FUNCTION = 0,

	// Loops
	K_FOR = 1,
	K_WHILE = 2,

	// Logical operators
	K_AND = 3,
	K_OR = 4,
	K_NOT = 5,

	// Return keyword
	K_RETURN = 6,

	// Import keyword
	K_IMPORT = 7,

	// Type decl keyword
	K_TYPE = 8,

	// If-else keywords
	K_IF = 9,
	K_ELSE = 10,
} keyword_t;

bool is_keyword(const char *);
keyword_t get_keyword(const char *);
