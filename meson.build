project('nextgen', 'c', version: '0.1')

includes = include_directories('include')

sources = [
  'src/nxg.c',
  'src/tokenize.c',
  'src/file.c',
  'src/error.c',
  'src/parser.c',
  'src/type.c',
  'src/generate.c'
]

llvm = dependency('llvm')

executable(
  'nxg', sources,
  include_directories: includes,
  cpp_args: ['-O0', '-g'],
  link_args: ['-fsanitize=address', '-Wall', '-Wextra', '-pedantic'],
  dependencies: llvm
)
