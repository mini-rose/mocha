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
	const char *root;

	/* pkgconfig */
	const char *pkgname;
	const char *pkgver;
	const char *source;
	const char *output;
	const char *outdir;

	action_t action;
	build_t build_type;

	char opt;
	char *options;
	bool verbose;
	bool quiet;
} settings_t;

#include <mocha/tools/argparse.h>
#include <mocha/tools/config.h>
#include <mocha/tools/project.h>

#define MOCHA_MAJOR 0
#define MOCHA_MINOR 1
#define MOCHA_TARGET "linux-x86_64"

#define DEFAULT_SETTINGS                                                       \
 {                                                                             \
  .build_type = B_DEBUG, .opt = '0', .verbose = false, .quiet = false,         \
  .action = A_HELP, .options = NULL, .source = NULL, .pkgname = NULL,          \
  .pkgver = NULL                                                               \
 }
