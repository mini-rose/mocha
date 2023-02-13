project('nextgen', 'c', version: '0.1')

includes = include_directories('include')

sources = [
  'src/nxg.c',
  'src/cc/tokenize.c',
  'src/cc/file.c',
  'src/cc/error.c',
  'src/cc/parser.c',
  'src/cc/parser/is.c',
  'src/cc/parser/free.c',
  'src/cc/parser/value.c',
  'src/cc/parser/call.c',
  'src/cc/parser/fn.c',
  'src/cc/parser.c',
  'src/cc/type.c',
  'src/cc/keyword.c',
  'src/cc/emit.c',
  'src/cc/module.c',
  'src/cc/mangle.c',
  'src/cc/build.c',
  'src/bs/buildfile.c'
]

llvm = declare_dependency(link_args: '-lLLVM')

executable(
  'nxg', sources,
  include_directories: includes,
  cpp_args: ['-O0', '-g'],
  link_args: ['-fsanitize=address', '-Wall', '-Wextra', '-pedantic'],
  dependencies: llvm
)
