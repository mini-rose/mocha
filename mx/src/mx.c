/* mx - mocha compiler, build system & package manager
   Copyright (c) 2023 mini-rose */

#include <getopt.h>
#include <mx/bs/buildfile.h>
#include <mx/cc/alloc.h>
#include <mx/mx.h>
#include <mx/opt.h>
#include <mx/pkg/pm.h>
#include <mx/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static settings_t settings;

static inline void help()
{
	puts("usage: mx [option]... [action/<input>]");
}

static inline void full_help()
{
	help();
	fputs(
	    "Compile, build and link mocha source code into an executable.\n\n"
	    "\033[1;34mActions\033[0m\n"
	    "\tnew <name>         create new project\n"
	    "\tbuild              build project\n"
	    "\trun                build and run project\n"
	    "\tclean              remove generated artifacts\n"
	    "\t.                  use .mocha buildfile\n\n"
	    "\033[1;34mOptions\033[0m\n"
	    "\t-o <path>          output binary file name "
	    "(default: " DEFAULT_OUT ")\n"
	    "\t-O <level>         optimization level, one of: 0 1 2 3 s\n"
	    "\t-p                 show generated AST\n"
	    "\t-r, --root <path>  mocha root path (default: " MX_ROOT ")\n"
	    "\t-t                 show generated tokens\n"
	    "\t-v, --version      show the compiler version\n"
	    "\t-V                 be verbose, show ran shell commands\n"
	    "\t-Xsanitize-alloc   sanitize the internal allocator\n"
	    "\t-Xalloc            dump allocation stats\n"
	    "\t-fno-builtins      don't include builtins\n"
	    "\n\033[1;34mLint\033[0m\n"
	    "\t-Wno-unused        unused variables\n"
	    "\t-Wno-random        random stuff that don't fit into any other "
	    "option\n"
	    "\t-Wno-empty-block   empty blocks\n"
	    "\t-Wno-prefer-ref    should pass a reference instead of a copy\n"
	    "\t-Wno-self-name     first method parameter should be named self\n"
	    "\n\033[1;34mLink\033[0m\n"
	    "\t--ldd <path>       dynamic linker to use\n"
	    "\n\033[1;34mEmit\033[0m\n"
	    "\t-Eno-stack         disable stacktrace\n"
	    "\t-Ekeep-var-names   keep variable names in LLVM IR\n",
	    stdout);
	exit(0);
}

static inline void version()
{
	printf("mx %d.%d\n", MX_MAJOR, MX_MINOR);
	printf("target: %s\n", MX_TARGET);
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
	settings->sysroot = MX_ROOT;

	if (access("/usr/lib/mocha", F_OK) == 0) {
		settings->sysroot = "/usr/lib/mocha";
	} else if (access("/usr/local/lib/mocha", F_OK) == 0) {
		settings->sysroot = "/usr/local/lib/mocha";
	} else {
		error("could not find mocha root path, please specify it with "
		      "-r");
	}

	settings->output = DEFAULT_OUT;
	settings->global = false;
	settings->input = NULL;
	settings->using_bs = false;
	settings->show_tokens = false;
	settings->dyn_linker = NULL;

	settings->verbose = false;

	/* Package manager */
	settings->pm = false;
	settings->pm_run = false;

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

	if (!strncmp(option, "root", 4))
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

	/* Important note: do NOT allocate any memory between here and the call
	   to slab_init_global(), as it will just crash. */

	atexit(exit_routines);
	default_settings(&settings);

	static struct option longopts[] = {{"help", no_argument, 0, 0},
					   {"version", no_argument, 0, 0},
					   {"ldd", required_argument, 0, 0},
					   {"root", required_argument, 0, 0},
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

	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], ".", 2)) {
			buildfile(&settings);
			settings.using_bs = true;
		}

		if (!strncmp(argv[i], "new", 4)) {
			if (!argv[i + 1])
				error("expected package name after 'new'.");
			pm_create_pkg(argv[i + 1]);
			exit(0);
		}

		if (!strncmp(argv[i], "build", 6))
			pm_build(&settings);

		if (!strncmp(argv[i], "run", 4))
			pm_run(&settings);

		if (!strncmp(argv[i], "clean", 4))
			pm_clean(&settings);
	}

	if (settings.input == NULL) {
		if (optind >= argc)
			error("missing source file name");
		settings.input = slab_strdup(argv[optind]);
	}

	int n = strlen(settings.sysroot);
	if (settings.sysroot[n - 1] == '/')
		settings.sysroot[n - 1] = 0;

	// Check settings
	validate_settings(settings);

	compile(&settings);

	return 0;
}
