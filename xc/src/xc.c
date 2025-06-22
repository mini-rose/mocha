/* xc - x compiler, build system & package manager
   Copyright (c) 2023 mini-rose */

#include "xc.h"

#include "alloc.h"
#include "opt.h"
#include "utils/error.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static settings_t settings;

static inline void help()
{
	puts("usage: xc [option]... [action/<input>]");
}

static inline void full_help()
{
	help();
	fputs("  -o <file>           output (default: " DEFAULT_OUT ")\n"
	      "  -O <lvl>            opt: 0 1 2 3 s\n"
	      "  -p                  print AST\n"
	      "  -t                  print tokens\n"
	      "  --sysroot <path>    sysroot\n"
	      "  --ldd <path>        dynamic linker\n"
	      "  -v                  version\n"
	      "  --version           version\n"
	      "  -V                  verbose\n"
	      "  -Xsanitize-alloc    alloc sanitizer\n"
	      "  -Xalloc             alloc stats\n"
	      "  -fno-builtins       no builtins\n"
	      "  -Wno-unused         unused vars\n"
	      "  -Wno-random         random warn\n"
	      "  -Wno-empty-block    empty blocks\n"
	      "  -Wno-prefer-ref     prefer ref\n"
	      "  -Wno-self-name      self param\n"
	      "  -Eno-stack          no stacktrace\n"
	      "  -Ekeep-var-names    keep var names\n",
	      stdout);
	exit(0);
}

static inline void version()
{
	printf("xc %d.%d\n", XC_MAJOR, XC_MINOR);
	printf("target: %s\n", XC_TARGET);
	printf("root: %s\n", settings.sysroot);

	if (OPT_ALLOC_SLAB_INFO || OPT_DEBUG_INFO) {
		printf("opt: ");

		if (OPT_ALLOC_SLAB_INFO)
			fputs("alloc-slab-info ", stdout);
		if (OPT_DEBUG_INFO)
			fputs("debug-info ", stdout);

		fputc('\n', stdout);
	}
	exit(0);
}

static inline void default_settings(settings_t *settings)
{
	/* Note: Allocating memory here will crash, as the slab alloc is not
	   initialized yet. */
	settings->sysroot = XC_ROOT;

	settings->output = DEFAULT_OUT;
	settings->global = false;
	settings->input = NULL;
	settings->using_bs = false;
	settings->show_tokens = false;
	settings->dyn_linker = NULL;

	settings->verbose = false;

	/* Emit */
	settings->emit_stacktrace = true;
	settings->emit_varnames = false;

	/* Warnings */
	settings->warn_unused = true;
	settings->warn_random = true;
	settings->warn_empty_block = true;
	settings->warn_prefer_ref = true;
	settings->warn_self_name = true;

	/* Extra */
	settings->x_sanitize_alloc = false;
	settings->x_dump_alloc = false;

	/* Features */
	settings->f_builtins = true;

	settings->opt = "0";
}

static inline void validate_settings(settings_t settings)
{
	if (settings.sysroot == NULL)
		error("missing sysroot path");

	if (access(settings.sysroot, F_OK) == -1) {
		error("sysroot path does not exist: %s, see help for more "
		      "information",
		      settings.sysroot);
	}

	if (settings.dyn_linker && access(settings.dyn_linker, F_OK) == -1) {
		error(
		    "dynamic linker path does not exist: %s, see help for more "
		    "information",
		    settings.dyn_linker);
	}
}

void parse_opt(settings_t *settings, const char *option, char *arg)
{
	if (!strncmp(option, "help", 4))
		full_help();

	if (!strncmp(option, "version", 8))
		version();

	if (!strncmp(option, "ldd", 3))
		settings->dyn_linker = arg;

	if (!strncmp(option, "sysroot", 7))
		settings->sysroot = arg;
}

void parse_emit_opt(settings_t *settings, const char *option)
{
	if (!strcmp("no-stack", option))
		settings->emit_stacktrace = false;
	else if (!strcmp("keep-var-names", option))
		settings->emit_varnames = true;
	else
		warning("unknown emit option `%s`", option);
}

void parse_extra_opt(settings_t *settings, const char *option)
{
	if (!strcmp("sanitize-alloc", option))
		settings->x_sanitize_alloc = true;
	else if (!strcmp("alloc", option))
		settings->x_dump_alloc = true;
	else
		warning("unknown extra option `%s`", option);
}

void parse_warn_opt(settings_t *settings, const char *option)
{
	bool true_or_false;

	true_or_false = true;

	if (!strncmp(option, "no-", 3)) {
		true_or_false = false;
		option += 3;
	}

	if (!strcmp("unused", option))
		settings->warn_unused = true_or_false;
	else if (!strcmp("random", option))
		settings->warn_random = true_or_false;
	else if (!strcmp("empty-block", option))
		settings->warn_empty_block = true_or_false;
	else if (!strcmp("prefer-ref", option))
		settings->warn_prefer_ref = true_or_false;
	else if (!strcmp("self-name", option))
		settings->warn_self_name = true_or_false;
	else
		warning("unknown warn option `%s`", option);
}

void parse_feature_opt(settings_t *settings, const char *option)
{
	bool true_or_false;

	true_or_false = true;

	if (!strncmp(option, "no-", 3)) {
		true_or_false = false;
		option += 3;
	}

	if (!strcmp("builtins", option))
		settings->f_builtins = true_or_false;
	else
		warning("unknown feature option `%s`", option);
}

static void exit_routines()
{
	if (settings.x_dump_alloc)
		alloc_dump_stats();
	if (settings.x_sanitize_alloc)
		slab_sanitize_global();
	slab_deinit_global();
}

int main(int argc, char **argv)
{
	int c, optindx = 0;

	/* Important note: do NOT allocate any memory between here and
	   the call to slab_init_global(), as it will just crash. */

	atexit(exit_routines);
	default_settings(&settings);

	static struct option longopts[] = {{"help", no_argument, 0, 0},
					   {"version", no_argument, 0, 0},
					   {"ldd", required_argument, 0, 0},
					   {"sysroot", required_argument, 0, 0},
					   {"alloc", no_argument, 0, 0}};

	while (1) {
		c = getopt_long(argc, argv, "o:r:O:E:W:X:f:hvptV", longopts,
				&optindx);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			parse_opt(&settings, longopts[optindx].name, optarg);
			break;
		case 'o':
			settings.output = optarg;
			break;
		case 'r':
			settings.sysroot = optarg;
			break;
		case 'h':
			full_help();
			break;
		case 'p':
			settings.show_ast = true;
			break;
		case 't':
			settings.show_tokens = true;
			break;
		case 'v':
			version();
			break;
		case 'V':
			settings.verbose = true;
			break;
		case 'O':
			settings.opt = optarg;
			break;
		case 'X':
			parse_extra_opt(&settings, optarg);
			break;
		case 'E':
			parse_emit_opt(&settings, optarg);
			break;
		case 'W':
			parse_warn_opt(&settings, optarg);
			break;
		case 'f':
			parse_feature_opt(&settings, optarg);
			break;
		}
	}

	slab_init_global(settings.x_sanitize_alloc);

	int n = strlen(settings.sysroot);
	if (settings.sysroot[n - 1] == '/')
		settings.sysroot[n - 1] = 0;

	// Check settings
	validate_settings(settings);

	if (settings.input == NULL) {
		if (optind >= argc)
			error("missing source file name");
		settings.input = slab_strdup(argv[optind]);
	}

	compile(&settings);

	return 0;
}
