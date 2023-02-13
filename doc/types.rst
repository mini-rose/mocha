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
