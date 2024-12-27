/* buildfile.h - parse the build system file
   Copyright (c) 2023 mini-rose */
#pragma once

#include <mx/mx.h>

/**
 * Simple parser for mocha build system.
 * Puts options into global settings_t.
 */
void buildfile(settings_t *settings);
