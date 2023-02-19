Build system
============

Coffee has its own build system, which is baked into the compiler for
convenience. It is all based around the .coffee file, which is located
in the project root directory.


Options
-------
* ``source`` - source file
* ``output`` - executable file name
* ``flags`` - array of one character options for example: ['-p', '-t']
* ``sysroot`` - coffee root path, like /usr/lib/coffee (see nxg -v for the default)


Example
-------

Here is an example .coffee file::

        source = 'main.ff'
        output = 'main'
        sysroot = '../'
        flags = [ '-p', '-Eno-stack', '-O3' ]
