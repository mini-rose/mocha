project(
  'mocha', 'c', version: '0.1.0',
  default_options: [
    'prefix=/usr',
    'c_std=c2x',
    'bindir=bin'
  ]
)

sources = [
  # Main
  'src/mocha.c',

  # Tools
  'src/tools/argparse.c',
  'src/tools/project.c',
  'src/tools/config.c',

  # Utils
  'src/utils/utils.c',
  'src/utils/error.c',
  'src/utils/file.c'
]

includes = include_directories('include')
bindir = get_option('prefix') / get_option('bindir')

c_args = [
  '-Wall', '-Wextra'
]

link_args = []

if get_option('buildtype') == 'debug'
  c_args += ['-O0', '-ggdb', '-fsanitize=address', '-DDEBUG']
  link_args += '-fsanitize=address'
else
  c_args += '-O3'
endif

exe = executable(
  'mocha', sources,
  include_directories: includes,
  c_args: c_args,
  link_args: link_args,
  install: true,
  install_dir: bindir
)
