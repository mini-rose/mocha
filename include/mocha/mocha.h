/* mocha.h
   Copyright (c) 2023 mini-rose */

#pragma once

typedef enum
{
	A_NEW,
	A_RUN,
	A_INIT,
	A_HELP,
	A_BUILD,
	A_CLEAN,
	A_VERSION,
} action_t;

typedef enum
{
	B_DEBUG,
	B_RELEASE
} build_t;

typedef struct {
	/* Project root directory */
	const char *root;

	/* Project source directory */
	const char *src;
	const char *src_main;

	/* Project build directory */
	const char *out;

	/* Package information */
	const char *package_name;
	const char *package_version;

	/* Workflow */
	action_t action;
	bool quiet;
} settings_t;

#define DEFAULT_SETTINGS                                                       \
 {                                                                             \
  .root = NULL, .src = NULL, .out = NULL, .package_name = NULL,                \
  .package_version = NULL, .action = A_HELP, .quiet = false, .src_main = NULL  \
 }
