Types
=====


Integers
--------

- ``bool`` true/false, represented internally as a LLVM i1
- ``i8`` -128 to 127
- ``i16`` -32768 to 32767
- ``i32`` -2147483648 to 2147483647
- ``i64`` -9223372036854775808 to 9223372036854775807
- ``f32`` ±3.40282347 x 10^38 and a precision of around 7 decimal digits.
- ``f64`` ±1.7976931348623157 x 10^308 and a precision of around 15 decimal digits.


String
------

Coffee provides a built-in dynamically-allocated string type ``str``. Creating
and destroying strings is handled by the compiler, so it can seem like a simple
type you can use without thinking about allocating enough memory for it.

The implementation should call different methods depending on the operation.
Assigning a value to a newly created variable generates 2 separate expressions
internally, an E_VARDECL and E_ASSIGN. This means that declaring the variable
basically creates an ``alloca`` to store the struct, and then initializes it.

The assignment will then make a copy of the object on the right::

        initializes a variable with the empty str type
        v
        x: str

            this literal will get turned into a const struct of str
            v
        x = "asdf"
          ^
          which will then get copied into the `x` value

This is roughly how it should look like in LLVM IR, in reality there would be more
metadata about the variables itself, like the alignment::

        ; Allocate space for the str instance
        %x = alloca %str

        ; Zero-initialize the str
        store { i64, ptr, i32 } zeroinitializer, ptr %x

        ; Create a temporary literal with the const global string
        %l = alloca %str
        store %str { i64 4, ptr @0, i32 0 }, ptr %l

        ; Copy the temporary string into the actual variable
        call void @_C4copyP3strP3str(ptr %x, ptr %l)

If the string goes out of scope, the matching drop function will get emitted.
The compiler depends on the std.builtin.string module to provide these
functions which it can then emit calls to.


Structured type
---------------

Similar to other programming languages, Coffee offers an structured type, which
is basically the same as the C struct. You can create an object type using the
``type`` keyword::

        type User {
                name: str
                id: i32
        }

This type has two fields, a name and id, which can both be accessed by anyone,
as there isn't any notion of private/protected fields. In LLVM IR, this would be
represented as::

        %User = type { %str, i32 }

Structures have special semantic rules emitted by the compiler. For example,
passing a raw object to a function (not a reference to it) will generate a copy
call before passing the value. This will require a function defined for that
operation. Here is a list of functions that shall be implemented for an object::

        fn copy<T>(self: &T, from: &T) -> null
        fn drop<T>(self: &T) -> null

Note that if the compiler does not find a matching copy or drop function, it
will try to generate one on its own. Objects with plain types will not get
a drop function, because they don't allocate any memory.
