project('static-web', 'c', default_options: [ 'b_ndebug=if-release' ])

src = [
  'web-cc.c',
  'unicode.c',
  'html_tokenizer.c'
]

include = include_directories('.')
thread_dep = dependency('threads')

add_global_arguments('-march=native', language: 'c')
executable('web-cc', sources : src, include_directories : include, dependencies: thread_dep)
