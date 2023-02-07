#pragma once

#define bool _Bool

typedef enum
{
	// Function declaration keyword
	K_FUNCTION = 0,

	// Control flow statements
	K_IF = 1,
	K_ELIF = 2,
	K_ELSE = 3,

	// Loops
	K_FOR = 4,
	K_WHILE = 5,

	// Logical operators
	K_AND = 6,
	K_OR = 7,
	K_NOT = 8,

	// Return keyword
	K_RETURN = 9,

	// Import keyword
	K_IMPORT = 10
} keyword_t;

bool is_keyword(const char *);
keyword_t get_keyword(const char *);
