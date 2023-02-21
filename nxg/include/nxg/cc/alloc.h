/* alloc.h - custom allocators for nxg
   Copyright (c) 2023 mini-rose */

#pragma once
#include <stddef.h>

/* 16KB slabs */
#define SLAB_SIZE 0x4000

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
};

void slab_init_global();
void slab_deinit_global();

void slab_create(struct slab *allocator, int block_size);
void slab_destroy(struct slab *allocator);

void *slab_alloc(int n);
void *slab_alloc_blocks(int n, int block_size);

void *slab_strdup(const char *str);
void *slab_strndup(const char *str, size_t len);

void *realloc_ptr_array(void *array, int n);

void alloc_dump_stats();
