/* pm.h - package manager
   Copyright (c) 2023 mini-rose */

#pragma once

#include <mx/mx.h>

void pm_create_pkg(const char *name);
void pm_build(settings_t *settings);
void pm_run(settings_t *settings);
void pm_clean(settings_t *settings);
