/* tools/config.h
   Copyright (c) 2023 mini-rose */

#ifndef _TOOLS_H
# error                                                                         \
     "Never include <mocha/tools/config.h> directly; use <mocha/tools.h> instead"
#endif

#include <mocha/mocha.h>

/*
   Parses a buildfile options to settings_t.

   @see: mocha/mocha.h for more information
*/
void cfgparse(settings_t *settings);
