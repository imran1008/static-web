project('static-web', 'c', default_options: [ 'b_ndebug=if-release' ])

src = [
  'web_cc.c',
  'unicode.c',
  'html_lexer.c',
  'html_parser.c'
]

include = include_directories('.')
thread_dep = dependency('threads')

add_global_arguments('-march=native', language: 'c')
executable('web-cc', sources : src, include_directories : include, dependencies: thread_dep)
