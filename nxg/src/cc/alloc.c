/* alloc.c - custom allocators for nxg
   Copyright (c) 2023 mini-rose */

#include <nxg/cc/alloc.h>
#include <nxg/utils/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct alloc_entry
{
	void *block;
	int size;
};

struct alloc_table
{
	struct alloc_entry **allocs;
	int n_allocs;
};

struct array
{
	void **items;
	int n_items;
};

struct array_table
{
	struct array **arrays;
	int n_arrays;
};

struct slab_allocators
{
	struct slab *slabs;
	int n_slabs;
};

struct slab_allocators global_slabs;

struct alloc_table global_alloc;
struct array_table global_arrays;

void slab_init_global()
{
	global_slabs.n_slabs = 6;
	global_slabs.slabs = calloc(global_slabs.n_slabs, sizeof(struct slab));
	slab_create(&global_slabs.slabs[0], 8);
	slab_create(&global_slabs.slabs[1], 16);
	slab_create(&global_slabs.slabs[2], 32);
	slab_create(&global_slabs.slabs[3], 64);
	slab_create(&global_slabs.slabs[4], 512);
	slab_create(&global_slabs.slabs[5], 1024);
}

void slab_deinit_global()
{
	for (int i = 0; i < global_alloc.n_allocs; i++)
		free(global_alloc.allocs[i]->block);
	free(global_alloc.allocs);

	for (int i = 0; i < global_arrays.n_arrays; i++)
		free(global_arrays.arrays[i]->items);
	free(global_arrays.arrays);

	/* The slabs must be free'd at the very end. */
	for (int i = 0; i < global_slabs.n_slabs; i++)
		slab_destroy(&global_slabs.slabs[i]);
}

void slab_create(struct slab *allocator, int block_size)
{
	memset(allocator, 0, sizeof(*allocator));
	allocator->block_size = block_size;
	allocator->blocks_per_slab = SLAB_SIZE / block_size;

	allocator->slabs = calloc(1, sizeof(void *));
	allocator->slabs[0] = malloc(SLAB_SIZE);
	allocator->n_slabs = 1;
}

void slab_destroy(struct slab *allocator)
{
	for (int i = 0; i < allocator->n_slabs; i++)
		free(allocator->slabs[i]);
	free(allocator->slabs);

	memset(allocator, 0, sizeof(*allocator));
}

static void *slab_acquire_block(struct slab *self, int requested)
{
	void *base_ptr;
	void *block_ptr;
	int blocks;

	if (self->i_slab == self->n_slabs) {
		self->slabs =
		    realloc(self->slabs, sizeof(void *) * (self->n_slabs + 1));
		self->slabs[self->n_slabs++] = malloc(SLAB_SIZE);
	}

	base_ptr = self->slabs[self->i_slab];
	block_ptr = base_ptr + (self->block_size * self->i_pos);

	self->i_pos++;
	if (self->i_pos == self->blocks_per_slab) {
		self->i_slab++;
		self->i_pos = 0;
	}

	/* Stats */
	blocks = self->i_slab * self->blocks_per_slab + self->i_pos;

	self->unused_padding += self->block_size - requested;
	self->average_block = (self->average_block * (blocks - 1)) + requested;
	self->average_block /= blocks;

	memset(block_ptr, 0, self->block_size);
	return block_ptr;
}

static void *acquire_oversized_block(int n)
{
	void *block;

	block = calloc(1, n);

	global_alloc.allocs =
	    realloc(global_alloc.allocs,
		    sizeof(struct alloc_entry *) * (global_alloc.n_allocs + 1));
	global_alloc.allocs[global_alloc.n_allocs] =
	    slab_alloc(sizeof(struct alloc_entry));
	global_alloc.allocs[global_alloc.n_allocs]->block = block;
	global_alloc.allocs[global_alloc.n_allocs]->size = n;

	global_alloc.n_allocs++;

	return block;
}

void *slab_alloc(int n)
{
	struct slab *allocator = NULL;

	for (int i = 0; i < global_slabs.n_slabs; i++) {
		if (n <= global_slabs.slabs[i].block_size) {
			allocator = &global_slabs.slabs[i];
			break;
		}
	}

	if (!allocator)
		return acquire_oversized_block(n);

	return slab_acquire_block(allocator, n);
}

void *slab_alloc_array(int n, int item_size)
{
	return slab_alloc(n * item_size);
}

void *slab_strdup(const char *str)
{
	return slab_strndup(str, strlen(str));
}

void *slab_strndup(const char *str, size_t len)
{
	char *ptr;

	ptr = slab_alloc(len + 1);
	memcpy(ptr, str, len);
	ptr[len] = 0;

	return ptr;
}

static void *array_create(int n)
{
	struct array *arr;

	arr = slab_alloc(sizeof(*arr));

	global_arrays.arrays =
	    realloc(global_arrays.arrays,
		    (global_arrays.n_arrays + 1) * sizeof(struct array *));
	global_arrays.arrays[global_arrays.n_arrays++] = arr;

	arr->items = calloc(n, sizeof(void *));
	arr->n_items = n;

	return arr->items;
}

static struct array *array_find(void *ptr)
{
	for (int i = 0; i < global_arrays.n_arrays; i++) {
		if (global_arrays.arrays[i]->items == ptr)
			return global_arrays.arrays[i];
	}

	return NULL;
}

void *realloc_ptr_array(void *array, int n)
{
	struct array *self;

	if (array == NULL)
		return array_create(n);

	/* Re-allocate the array */
	self = array_find(array);
	self->items = realloc(self->items, n * sizeof(void *));
	self->n_items = n;

	return self->items;
}

static int slab_dump(struct slab *self)
{
	float used;
	float padding;
	int blocks;
	int total;

	blocks = self->i_slab * self->blocks_per_slab + self->i_pos;
	used = (float) blocks / (self->n_slabs * self->blocks_per_slab) * 100;
	total = (self->n_slabs * SLAB_SIZE) / 1024;
	padding =
	    ((float) self->unused_padding / (blocks * self->block_size)) * 100;

	printf("alloc: % 10d % 10d % 8d % 5.0f%% % 5.0f%% % 6.0f % 6d KB\n",
	       self->block_size, self->n_slabs, blocks, used, padding,
	       self->average_block, total);

	return total;
}

void alloc_dump_stats()
{
	int total = 0;
	int total_ptrs = 0;

	printf("alloc: Slab allocators:\n");
	printf("alloc:   slab size: %d (%d KB)\n", SLAB_SIZE, SLAB_SIZE / 1024);
	printf("alloc:   allocators in use: %d\n", global_slabs.n_slabs);
	printf("alloc:\nalloc: %10s %10s %8s %6s %6s %6s %9s\n", "block-size",
	       "slabs-used", "blocks", "used", "pad", "avg", "total");

	for (int i = 0; i < global_slabs.n_slabs; i++)
		total += slab_dump(&global_slabs.slabs[i]);

	printf("alloc:\nalloc: % 58d KB\n", total);

	printf("alloc:\nalloc: Oversized allocations:\n");
	printf("alloc: %d instance(s)\n", global_alloc.n_allocs);
	for (int i = 0; i < global_alloc.n_allocs; i++)
		printf("alloc:   %d B\n", global_alloc.allocs[i]->size);

	for (int i = 0; i < global_arrays.n_arrays; i++)
		total_ptrs += global_arrays.arrays[i]->n_items;

	printf("alloc:\nalloc: Pointer arrays:\n");
	printf("alloc:   %d instance(s), with a total of %d pointer(s)\n",
	       global_arrays.n_arrays, total_ptrs);
}
