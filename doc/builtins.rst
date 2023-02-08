Built-ins
=========

There are a number of built-in function-like expressions which allow the
programmer to do a number of things. Here is an exhaustive list of all
of them implemented:

__builtin_decl
--------------

Declare a function into the global scope. This works similar to forward
function declaration in C::

                        function name in string
                        |                return type of function
                        v                v
        __builtin_decl("is_larger_than", bool, i32, i32)
                                               ^
                                               type of arguments...

You can then call the function normally, as it would be defined somewhere
in your file::

        print(is_larger_than(4, 5))


Important note: the first argument to __builtin_decl is a function name,
which is **not mangled** with the following types. This means that the
declared function must be marked as no_mangle. If you're trying to call
C code, you don't have to worry about that as functions are not mangled
anyway. If you implement the `is_larger_than` function in Coffee, it would
get mangled as `_C14is_larger_thanii`.
