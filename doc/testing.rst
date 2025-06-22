Testing
=======

Every language and standard library feature should be covered by a test, but we
all know it's sometimes just not possible or too much of a hassle. Writing some
tests for x is really simple, as you just need to really create a directory
and a file, and the rest will be managed by `xtest`, a Python script which
manages all the test suites.

Let's create a simple test for checking if 2 + 2 is actually 4. First, select
a proper group for your test. As we're checking something that is built into
the language, we should put it into the `language` group::

        If we're in the project root:

        $ cd test/language
        $ mkdir two-plus-two
        $ cd two-plus-two

Once we've created a directory in the proper group, we'll need some code to run
and a result file to compare against. Create a ``test.x`` file::

        use std.io

        fn main {
                i = 2 + 2
                print(i)
        }

This program should print '4' to stdout. Comparing stdout to a .result file is
the basis of all tests in x. To create a result file, you may simply::

        $ echo '4' > test.result

Now, leave the directory and return to the test directory, and run the tests,
where we can see our test passing. If it happens to fail, it will plant a big
red FAIL message::

        $ cd ../../
        $ ./xtest
        ...
          running language.two-plus-two OK
        ...

You can try changing the value in test.result, to see what happens if the test
fails.


Intentional failure
-------------------

If you want to check if the compiler will fail on a syntax error for example,
you want to mark the test as one that is supposed to fail::

        # test.info
        fails=true


Silence warnings
----------------

A warning about an unused variable will mark the test as failed, so you might
want to silence it to make it pass. You can simply specify compiler flags with
warnings you might want to ignore, in the test.info file::

        # test.info
        flags=-Wno-unused
