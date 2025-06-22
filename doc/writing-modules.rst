Writing modules
===============

Modules are simply Mocha source code files which are imported by the compiler
if the user requests so using the ``import`` keyword. Note that some modules are
automatically imported by the compiler to support with code generation. Here
is a simple "hello world" style example:

Start by creating a hello.x file, where we will implement a function::

        fn hello {
                print("hello")
        }

In the main module, named here "main.x" add a use statement::

        import "hello"

        fn main {
                hello()
        }

After we compile this program by just calling ``xc main.x``, we can see that
the compiler automatically sorts out all imports and additional modules.


Extending with C
----------------

You can very easily extend Mocha programs with C code by writing additional
modules for it, and simply declaring that such functions exist. Here is an
add function implemented in C.

First, tell the compiler that an add functions exists, which returns an i32
and takes 2 arguments, both being plain i32::

        // add.x

        __builtin_decl_mangled("add", i32, i32, i32)

**Note**: It is recommended to use the _mangled variant of the __builtin_decl
call, so that you may still use overloading writing C modules.

You can now create a C file, which will implement the add functions we just
declared. There are a couple of requirements to having the compiler notice
a C source file:

  * The C file should have the same base name as the .x module
  * It should be placed in the same directory as the .x module

This means that if we want to implement the add function, we have to create
an add.c file in the same directory as the add.x module source file, so it
can get recognized as an additional dependency for the compiler::

        // add.c

            mangle the name appropriately (see mangling.rst)
            |        i32 in Mocha refers to the same type as an int in C
            v        v
        int _M3addii(int a, int b)
        {
                return a + b;
        }

Now, to test our module out create a main.x file in the same directory as
these modules::

        // main.x

        import "add"

        fn main {
                print(add(4, 5))
        }

The compiler will now look for a file named add.x in the local directory, and
then import it if it finds something. Then, it quickly checks if a source file
with a similar name but ending with .c is present, and builds an object from it
if that's the case (it will drop the object into the same directory, just with
a .o file ending). Now, just build this test with::

        $ xc main.x && ./a.out

and the compiler will do the rest.

This option is really powerful, as it allows for a gradual change from C to
Mocha, without the need to write everything at once as it's pretty much ABI
compatible. There is one really important thing to remember, which is structs
in Mocha and structs in C use different ABI rules, meaning, when passed by
value, they will result in **entirely different semantics, which break
compatibility**. The simple fix for this is implementing a jump-pad function
in Mocha, which just copies the value and passes it on by reference to C code.

Here is an example::

        // mod.x

        struct User {
                name: str
                id: i32
        }

        /* The jump-pad function to C land. */
        fn consume_user(user: User) {
                __builtin_decl("c_consume_user", null, &User)
                c_consume_user(&user)
        }

        // mod.c

        struct User {
                struct mo_name name;
                int id;
        };

        void c_consume_user(struct User *user) {
                // do something with the value...
        }

This is mostly because C and LLVM implement slightly different ABIs for passing
structs by-value, and also because this would result in undefined behaviour for
some object types, which the Mocha compiler would generate ``drop(&T)`` calls
for. In this case, the User type gets drop() called on it at the end of the
consume_user function in order to free the copied name field. It would be very
easy to forget to drop the value in C, and the compiler would lose track of the
copied variable, which might result in bugs.
