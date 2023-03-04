/* tools/config.h
   Copyright (c) 2023 mini-rose */

#pragma once

#include <mocha/mocha.h>

/*
   Parses a buildfile options to settings_t.

   @see: mocha/mocha.h for more information
*/
void cfgparse(settings_t *settings);
