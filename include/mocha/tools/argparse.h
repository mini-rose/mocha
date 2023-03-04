/* tools/argparse.h
   Copyright (c) 2023 mini-rose */

#pragma once

#include <mocha/mocha.h>

/*
   Parses a command line arguments to settings_t.

   @see: mocha/mocha.h for more information
*/
void argparse(int argc, char **argv, settings_t *settings);
