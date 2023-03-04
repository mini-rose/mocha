/* nxg - mocha compiler, build system & package manager
   Copyright (c) 2023 mini-rose */

#include <getopt.h>
#include <nxg/bs/buildfile.h>
#include <nxg/cc/alloc.h>
#include <nxg/nxg.h>
#include <nxg/opt.h>
#include <nxg/pkg/pm.h>
#include <nxg/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void help()
{
	puts("usage: nxg [option]... [action/<input>]");
}

static inline void full_help()
{
	help();
	fputs(
	    "Compile, build and link mocha source code into an executable.\n\n"
	    "\e[1;34mActions\e[0m\n"
	    "\tnew <name>         create new project\n"
	    "\tbuild              build project\n"
	    "\trun                build and run project\n"
	    "\tclean              remove generated artifacts\n"
	    "\t.                  use .mocha buildfile\n\n"
	    "\e[1;34mOptions\e[0m\n"
	    "\t-o <path>          output binary file name "
	    "(default: " DEFAULT_OUT ")\n"
	    "\t-O <level>         optimization level, one of: 0 1 2 3 s\n"
	    "\t-p                 show generated AST\n"
	    "\t-r, --root <path>  mocha root path (default: " NXG_ROOT ")\n"
	    "\t-t                 show generated tokens\n"
	    "\t-v, --version      show the compiler version\n"
	    "\t-V                 be verbose, show ran shell commands\n"
	    "\t-Xsanitize-alloc   sanitize the internal allocator\n"
	    "\t-Xalloc            dump allocation stats\n"
	    "\n\e[1;34mLint\e[0m\n"
	    "\t-Wno-unused        unused variables\n"
	    "\t-Wno-random        random stuff that don't fit into any other "
	    "option\n"
	    "\t-Wno-empty-block   empty blocks\n"
	    "\t-Wno-prefer-ref    should pass a reference instead of a copy\n"
	    "\t-Wno-self-name     first method parameter should be named self\n"
	    "\n\e[1;34mLink\e[0m\n"
	    "\t--musl             use musl instead of glibc\n"
	    "\t--ldd <path>       dynamic linker to use (default: " DEFAULT_LD
	    ")\n"
	    "\n\e[1;34mEmit\e[0m\n"
	    "\t-Eno-stack         disable stacktrace\n"
	    "\t-Ekeep-var-names   keep variable names in LLVM IR\n",
	    stdout);
	exit(0);
}

static inline void version()
{
	printf("nxg %d.%d\n", NXG_MAJOR, NXG_MINOR);
	printf("target: %s\n", NXG_TARGET);
	printf("root: %s\n", NXG_ROOT);
	printf("lld: %s\n", DEFAULT_LD);

	if (OPT_ALLOC_SLAB_INFO || OPT_DEBUG_INFO || OPT_ASAN) {
		printf("opt: ");

		if (OPT_ALLOC_SLAB_INFO)
			fputs("alloc-slab-info ", stdout);
		if (OPT_DEBUG_INFO)
			fputs("debug-info ", stdout);
		if (OPT_ASAN)
			fputs("asan ", stdout);

		fputc('\n', stdout);
	}
	exit(0);
}

static inline void default_settings(settings_t *settings)
{
	/* Note: Allocating memory here will crash, as the slab alloc is not
	   initialized yet. */
	settings->sysroot = NXG_ROOT;
	settings->output = DEFAULT_OUT;
	settings->global = false;
	settings->input = NULL;
	settings->using_bs = false;
	settings->show_tokens = false;
	settings->dyn_linker = DEFAULT_LD;
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

	settings->opt = "0";
}

void parse_opt(settings_t *settings, const char *option, char *arg)
{
	if (!strncmp(option, "help", 4))
		full_help();

	if (!strncmp(option, "version", 8))
		version();

	if (!strncmp(option, "musl", 4))
		settings->dyn_linker = LD_MUSL;

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
	if (!strcmp("no-unused", option))
		settings->warn_unused = false;
	else if (!strcmp("no-random", option))
		settings->warn_random = false;
	else if (!strcmp("no-empty-block", option))
		settings->warn_empty_block = false;
	else if (!strcmp("no-prefer-ref", option))
		settings->warn_prefer_ref = false;
	else if (!strcmp("no-self-name", option))
		settings->warn_self_name = false;
	else
		warning("unknown warn option `%s`", option);
}

static settings_t settings;

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

	static struct option longopts[] = {
	    {"help", no_argument, 0, 0},       {"version", no_argument, 0, 0},
	    {"musl", no_argument, 0, 0},       {"ldd", required_argument, 0, 0},
	    {"root", required_argument, 0, 0}, {"alloc", no_argument, 0, 0}};

	while (1) {
		c = getopt_long(argc, argv, "o:r:O:E:W:X:hvptV", longopts,
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

	compile(&settings);

	return 0;
}
