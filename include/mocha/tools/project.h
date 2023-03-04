/* tools/project.h - basic project actions
   Copyright (c) 2023 mini-rose */

#pragma once

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
    Builds project with `mcc src/main.ff`
    and parsed options from the `.mocha.cfg`

    @see: mocha/tools/config.h for more information about `.mocha.cfg`
*/
void project_build(settings_t *settings);

/*
    Builds project and calls `builddir/main`

    @see: project_build for more informations
*/
void project_run(settings_t *settings);

/*
    Remove artifacts from the `builddir` directory
    that mocha has generated in past and all junk
    files from `mcc`.
*/
void project_clean(settings_t *settings);
