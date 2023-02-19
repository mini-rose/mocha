Errors
======

The compiler can issue a number of errors, which mostly are helpful as long as
they are in the parsing stage. The emit stage cannot produce pretty errors as
it does not have enough information. So, instead, it provides an error ID right
after "emit", which may look like this::

        error: emit[no-drop]: missing `fn drop(&File)`

This is a list of all error IDs:

* ``[no-copy]``
        Missing copy(&T, &T) function for object. When an object is assigned to
        another object, the compiler emits a copy() call for it, which needs to
        be implemented by the user.

* ``[no-drop]``
        When an object goes out of scope, drop(&T) is called on it. This error,
        similar to no-copy means that the compiler cannot find the implemented
        drop function.

* ``[type-mismatch]``
        An operation using atleast 2 types, for example assigning, cannot be
        emitted as the types do not match exactly (casting should be done on
        the compiling level, not by the emitter).

* ``[assign-dest]``
        The assignment destination cannot be assigned to, or the emitter does
        not know how to do it.

* ``[no-func]``
        There is no function with this name in the module or imported modules.

* ``[node-emit]``
        The emitter does not know how to emit a specific AST node. This is
        usually just a "not implemented yet" error.

* ``[llvm-ir]``
        The emitter failed to produce correct LLVM IR, which LLVM cannot
        understand and assemble.
