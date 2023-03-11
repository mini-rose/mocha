/* utils/file.h
   Copyright (c) 2023 mini-rose */

#pragma once

#ifndef _UTILS_H
# error                                                                         \
     "Never include <mocha/utils/file.h> directly; use <mocha/utils.h> instead"
#endif

typedef struct {
	const char *path;
	const char *content;
} file_t;

file_t *file_new(const char *path);
void file_destroy(file_t *file);
