Build system
============

Coffee has its own build system builtin compiler.

The `.coffee` file should be placed in project root
directory.


Options
-------
* `source` - source file
* `output` - executable file name
* `flags` - array of one character options for example: ['-p', '-t']
* `libpath` - standard library path


Example
-------

# comment
source = 'main.ff'
output = 'main'
libpath = '../lib'
flags = [ '-p', '-Eno-stack', '-O3' ]
