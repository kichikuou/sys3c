project('sys3c', 'c')
fs = import('fs')

sys3c_version = '0.3.0'

if host_machine.system() == 'emscripten'
  common_link_args = [
    '-sENVIRONMENT=node',
    '-sMODULARIZE',
    '-sEXIT_RUNTIME',
    '-sALLOW_MEMORY_GROWTH',
    '-sEXPORTED_RUNTIME_METHODS=FS,NODEFS',
    '-O' + get_option('optimization'),
    '-lnodefs.js'
  ]
else
  common_link_args = []
endif

#
# common
#

inc = include_directories('common')

common_srcs = [
  'common/ag00.c',
  'common/dri.c',
  'common/container.c',
  'common/game_id.c',
  'common/sjisutf.c',
  'common/util.c',
]

libcommon = static_library('common', common_srcs, include_directories : inc)
common = declare_dependency(include_directories : inc, link_with : libcommon, link_args : common_link_args)

common_tests_srcs = [
  'common/common_tests.c',
  'common/sjisutf_test.c',
  'common/util_test.c',
]

common_tests = executable('common_tests', common_tests_srcs, dependencies : common)
test('common_tests', common_tests, workdir : meson.current_source_dir())

#
# compiler
#

compiler_srcs = [
  'compiler/compile.c',
  'compiler/config.c',
  'compiler/debuginfo.c',
  'compiler/lexer.c',
  'compiler/sco.c',
]

libcompiler = static_library('compiler', compiler_srcs, dependencies : common)
compiler = declare_dependency(link_with : libcompiler)

sys3c_srcs = [
  'compiler/sys3c.c'
]
sys3c = executable('sys3c', sys3c_srcs, dependencies : [common, compiler], install : true)

#
# decompiler
#

sys3dc_srcs = [
  'decompiler/cali.c',
  'decompiler/decompile.c',
  'decompiler/sys3dc.c',
]
sys3dc = executable('sys3dc', sys3dc_srcs, dependencies : common, install : true)

#
# tools
#

dri = executable('dri', ['tools/dri.c'], dependencies : common, install : true)

#
# docs
#

docs = [
  ['docs/dri.adoc', ['dri.1']],
  ['docs/unicode.adoc', []],
  ['docs/sys3c.adoc', ['sys3c.1']],
  ['docs/sys3dc.adoc', ['sys3dc.1']],
]

asciidoctor = find_program('asciidoctor', required : false)

if asciidoctor.found()
  foreach doc : docs
    adoc = doc[0]
    manpages = doc[1]

    if manpages.length() > 0
      custom_target(manpages[0],
        output : manpages,
        input : adoc,
        command : [asciidoctor, '-b', 'manpage', '@INPUT@', '-o', '@OUTPUT0@',
                   '-a', 'sys3c-version=' + sys3c_version],
        install : true,
        install_dir : get_option('mandir') / 'man1')
    endif

    html = fs.replace_suffix(fs.name(adoc), '.html')
    custom_target(html,
      output : html,
      input : adoc,
      command : [asciidoctor, '@INPUT@', '-o', '@OUTPUT0@'],
      build_by_default : true)
  endforeach
endif
