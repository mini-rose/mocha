/* alloc.h - custom allocators for mx
   Copyright (c) 2023 mini-rose */

#pragma once
#include <stdbool.h>
#include <stddef.h>

/* See doc/mx/alloc.rst for more */

/* 16KB slabs */
#define SLAB_SIZE 0x4000

#define SLAB_MAGIC 0xee, 0x55

struct slab
{
	int block_size;
	int blocks_per_slab;
	int unused_padding;
	float average_block;
	int i_slab;
	int i_pos;
	void **slabs;
	int n_slabs;
	bool sanitize;
};

void slab_init_global(bool sanitize);
void slab_deinit_global();
void slab_sanitize_global();

void slab_create(struct slab *allocator, int block_size, bool sanitize);
void slab_destroy(struct slab *allocator);
void slab_sanitize(struct slab *allocator);

#if defined OPT_ALLOC_SLAB_INFO
# define slab_alloc(N) slab_alloc_info((N), __FILE__, __func__, __LINE__)
#else
# define slab_alloc(N) slab_alloc_simple(N)
#endif

void *slab_alloc_simple(int n);
void *slab_alloc_info(int n, const char *file, const char *func, int line);
void *slab_alloc_array(int n, int item_size);

void *slab_strdup(const char *str);
void *slab_strndup(const char *str, size_t len);

void *realloc_ptr_array(void *array, int n);

void alloc_dump_stats();

void dump_memory_prefixed(void *addr, int amount, const char *prefix);
void dump_memory(void *start, int n);
