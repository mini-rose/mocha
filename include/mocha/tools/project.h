/* tools/project.h - basic project actions
   Copyright (c) 2023 mini-rose */

#ifndef _TOOLS_H
# error                                                                         \
     "Never include <mocha/tools/project.h> directly; use <mocha/tools.h> instead"
#endif

#include <mocha/mocha.h>

/*
    Legend:
       * root path - provided by user by `mocha new (path)`
       * .mocha.cfg - @see: mocha/tools/config.h
       * builddir - field (output) in .mocha.cfg that means
		    directory where `mcc` outputs binary file.
*/

/*
    Creates project in root path with following file tree:
	root
	├ src/main.ff
	└ .mocha.cfg
*/
void project_new(settings_t *settings);

/*
    Initializes project in current working directory
    with following file tree:
	.
	├ src/main.ff
	└ .mocha.cfg
*/
void project_init(settings_t *settings);

/*
    Remove artifacts from the `builddir` directory
    that mocha has generated in past and all junk
    files from `mcc`.
*/
void project_clean(settings_t *settings);
