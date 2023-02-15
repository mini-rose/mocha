Standard library
================

This is an exhaustive list of the functions in the standard library, found in
the lib/std directory. Note that the whole standard library isn't required to
be even included, only the std.builtin part. Calls to functions from builtin
are emitted by the compiler itself, to support it with basic operations like
implementing the ``str`` type.

**Note:** All modules from the std.builtin path are imported automatically,
meaning you get access to stuff like ``print`` without the need to include
it.


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

* ``copy(&str, &str) -> null``
        Called by the compiler, when assigning one string to another. This
        means that all assignments are actually copies.

* ``len(&str) -> i64``
        Returns the string length.

* ``drop(&str) -> null``
        Called when a string goes out of scope.


std.builtin.list
----------------
* ``copy(&str, &str) -> null``
        Called by the compiler, when assigning one list to another. This
        means that all assignments are actually copies.

* ``len(&str) -> i64``
        Returns the list length.

* ``drop(&str) -> null``
        Called when a list goes out of scope.

* ``append(&list, &list) -> null``
        Append list by list.

* ``clear(&list) -> null``
        Removes all list items.

* ``remove(&list, u32) -> null``
        Removes item at given position.
