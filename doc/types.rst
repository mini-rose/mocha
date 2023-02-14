=====
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

Note: unsigned versions of i8-i128 are called i<n>

String
------

Coffee provides a built-in dynamically-allocated string type ``str``. Creating
and destroying strings is handled by the compiler, so it can seem like a simple
type you can use without thinking about allocating enough memory for it.

The implementation should call different methods depending on the operation.
Assigning a value to a newly created variable generates 2 seperate expressions
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

This is roughly should look like in LLVM IR::

        ; Allocate space for the str instance
        %x = alloca %str

        ; Zero-initialize the str
        store { i64, ptr, i32 } zeroinitializer, ptr %x

        ; Create a temporary literal with the const global string
        %l = alloca %str
        store %str { i64 4, ptr @0, i32 0 }, ptr %l

        ; Copy the temporary string into the actual variable
        call void @cf_strcopy(ptr %x, ptr %l)
