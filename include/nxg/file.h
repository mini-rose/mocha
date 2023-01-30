#pragma once

typedef struct {
	char *path;
	char *content;
} file;

file *file_new(const char *path);
void file_destroy(file *f);
