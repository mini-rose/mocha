=================
Function mangling
=================

Each function name gets mangled in order to support overloading and generic
types, similar to what C++ or Rust does with their code. Mocha uses the
``xc_mangle`` function, which uses the Itanium C++ ABI for mangling symbols
and type names (https://itanium-cxx-abi.github.io/cxx-abi/abi.html), but
the difference is in the second character; instead of a 'Z' we use a 'M',
so the main function will be mangled to ``_M4main`` instead of ``_M4main``.

Here, an example::

	add(i32 a, i64 b) -> _M3addil

This add function has 2 paramters, a regular int (i) and a 64-bit signed
integer (l), which get added at the end of the function name.

General construction::

	all symbols have the `_M` prefix
	       a list of parameter types
	v      v
	_M4namexxx
	   ^
	   len - name of the function


This means that ``add<T>(T a, T b)`` would get mangled differently depending
on the type of T. For example, add<i32>(...) would become ``_M3addii``, while
add<u128>(...) would become ``_M3addoo``.

Type methods get mangled with a prefix, showing the full path of the function.
For example::

        type User {
                print: fn (self: &User) { ... }
        }

                function name
                v
        _MN4User5printEP4User
           ^
           type name

These namespaced functions have the following structure::

        '_MN' len(type) <type> len(func) <func> 'E' <param-types>


Full table of the base types:

	========== ==========
	 **Type**    **ID**
	========== ==========
	  null       v
	  bool       b
	  i8         a
	  u8         h
	  i16        s
	  u16        t
	  i32        i
	  u32        j
	  i64        l
	  u64        m
	  i128       n
	  u128       o
	  f32        f
	  f64        d
	  str        3str
	========== ==========

Note that the return type does not get mangled.
