Standard library
================

This is an exhaustive list of the functions in the standard library, found in
the lib/std directory. Note that the whole standard library isn't required to
be even included, only the std.builtin part. Calls to functions from builtin
are emitted by the compiler itself, to support it with basic operations like
implementing the ``str`` type.


std.io
------

Routines for operating input/output streams.

Types

* ``type file_t``

Functions

* ``open(path: &str, mode: &str) -> file_t``
* ``open(path: str, mode: str) -> file_t``
* ``close(self: &file_t) -> null``

* ``write(self: &file_t, buf: str) -> null``
* ``write_stream(stream: i32, buf: str) -> null``

* ``print(i8) -> null``
* ``print(i32) -> null``
* ``print(i64) -> null``
* ``print(&str) -> null``
* ``print(bool) -> null``


std.os
------

* ``exit(i32) -> null``
        Cause normal process termination.


std.sys
-------

* ``getenv(str) -> str``
        Get an environment variable.

* ``system(str) -> null``
        Execute a shell command.

* ``sleep(i32) -> i32``
        Sleep for a specified number of seconds.

* ``version() -> &str``
        The version of compiler as a string


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


std.builtin.stacktrace
----------------------

Used by the compiler to emit stack information in the prologues and epilogues
of functions. These are not emitted if ``-Eno-stack`` is passed.

* ``__cf_stackpush(func: &i8, file_t: &i8) -> null #nomangle``
        Emitted at the start of a function.

* ``__cf_stackpop() -> null #nomangle``
        Emitted at the end of the function, just before the final ret
        instruction.
