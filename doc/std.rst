Standard library
================

This is an exhaustive list of the functions in the standard library, found in
the lib/std directory. Note that the whole standard library isn't required to
be even included, only the std.builtin part. Calls to functions from builtin
are emitted by the compiler itself, to support it with basic operations like
implementing the `str` type.

std.builtin.print
-----------------

Printing routines.

* ``print(i32) -> null``
* ``print(i64) -> null``
* ``print(&str) -> null``

std.builtin.string
------------------

Calls to these functions are emitted by the compiler for basic string
operations. For user-side string manipulation, see std.string instead.

* ``cf_strset(&str, &i8, u64) -> null``
* ``cf_strcopy(&str, &str) -> null``
* ``cf_strlen(&str) -> i64``
* ``cf_strdrop(&str) -> null``
