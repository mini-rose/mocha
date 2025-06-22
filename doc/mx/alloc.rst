Allocator API
=============

XC uses a set of custom allocators to make programming easier for the compiler
developer.


Global functions:

* ``slab_init_global(bool sanitize)``
        Initialize global allocators.

* ``slab_deinit_global()``
        Free global allocators.

* ``slab_sanitize_global()``
        Run the sanitizer on the global allocators. This effectively calls
        slab_sanitize() on every slab.


Allocation routines. As you can see, there is no slab_free routine. This is
because the compiler isn't a long-lived program, so freeing memory continuously
in this day and age is not really needed, meaning the slabs will get freed at
the end of the program, all at once.

* ``void *slab_alloc(int n)``
        Actually a macro, it finds a matching slab allocator, which means
        finding the smallest block size which N bytes will fit in. This means
        that calling slab_alloc(4) can allocate a block in a slab8 allocator.
        If the fitting allocator cannot be found, it will create an oversized
        allocation on the heap which will fit N bytes and no more.

* ``void *slab_alloc_array(int n, int item_size)``
        Allocates a contiguous array of n * item_size bytes.

* ``void *slab_strdup(const char *str)``
* ``void *slab_strndup(const char *str, size_t len)``
        Same as strdup() and strndup(), but allocated memory on the slab
        allocator.


Functions for operating on the ``struct slab`` allocator type. You should know
what you're doing if you want to create custom slab allocators, as the global
ones have additional functionality attached to them. If you want a new block
size, add a new allocator in slab_init_global.

* ``slab_create(struct slab *, int block_size, bool sanitize)``
        Create a new slab with the given block size and sanitizer option. This
        will initially allocate SLAB_SIZE (default 16KB) bytes to prepare for
        storing blocks. The chosen block_size should not be larger than 4KB to
        allow for sanitizing.

* ``slab_destroy(struct slab *)``
        Free all memory associated with this slab.

* ``slab_sanitize(struct slab *)``
        Run the sanitizer on this slab. Requires that the slab is created with
        the `sanitize` option enabled.
