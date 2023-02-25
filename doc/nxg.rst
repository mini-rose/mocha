nxg compiler
============

This document contains some topics about the details in nxg that are not
immediately understandable when looking at the code, or some design choices
that wouldn't be useful otherwise.


Memory allocation
-----------------

An important topic for any C program is memory allocations, which has become
a headache in nxg using basic malloc() and free() calls. This is why the slab
allocator was created, to have easy access to heap memory without the need to
worry about free'ing it. The slab allocators used in nxg do not allow for
free'ing memory, only acquiring. This means that once ``slab_deinit_global()``
has been called, all memory acquired by the slabs is free'd at once, removing
any worry about memory leaks.

Instead of::

        int *t = malloc(sizeof(int));

        // ...

        free(t);

Now, memory allocation is achieved using the ``slab_alloc`` function::

        int *t = slab_alloc(sizeof(int));

It will acquire a block in the slab with the best matching block size. For
example, if there are 4 slab allocators initialized with the following sizes:
8, 16, 32 and 64 - then an allocation of 7 bytes will get put into an 8 byte
block, and an allocation of over 64 bytes will get put into the oversize
section. For more information about all allocations done using slab_alloc,
call ``alloc_dump_stats()``.

The value acquired from slab_alloc is a pointer to a block, with a size of at
least n bytes, which is valid until the ``slab_deinit_global`` function gets
called.

Pointer arrays::

        struct object **array;
        int n;

        // Previous realloc calls:

        array = realloc(array, sizeof(struct object *) * (n + 1));

        // Should now use realloc_ptr_array instead.

                this will return a pointer to a void ** array with unallocated
                item blocks
                v
        array = realloc_ptr_array(array, n + 1);
                                         ^
                                         notice that we are not multiplying the
                                         elements by the item size

These will also get automatically free'd after calling slab_deinit_global.


Memory sanitizer
----------------

When passing ``-Xsanitize-alloc``, all global slab allocators will be created
with the sanitize option. This will use 3x as much memory, but will check for
buffer underflows and overflows. This is done by allocating 2 additional blocks
to the left and right of the user data block::

        |-underflow-|-user-block-|-overflow-|

Both the underflow and overflow block are filled with magic bytes, which are
then checked when calling ``slab_sanitize()``.
