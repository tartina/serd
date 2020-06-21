#!/usr/bin/env python

import glob
import os
import sys

from waflib import Build, Logs, Options
from waflib.extras import autowaf

# Library and package version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
SERD_VERSION       = '1.0.1'
SERD_MAJOR_VERSION = '1'

# Mandatory waf variables
APPNAME = 'serd'        # Package name for waf dist
VERSION = SERD_VERSION  # Package version for waf dist
top     = '.'           # Source directory
out     = 'build'       # Build directory

# Release variables
uri          = 'http://drobilla.net/sw/serd'
dist_pattern = 'http://download.drobilla.net/serd-%d.%d.%d.tar.bz2'
post_tags    = ['Hacking', 'RDF', 'Serd']


def options(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.load('python')
    opt = ctx.configuration_options()
    ctx.add_flags(
        opt,
        {'no-utils':     'do not build command line utilities',
         'no-python':    'do not build Python bindings',
         'no-cxx':       'do not build C++ bindings',
         'stack-check':  'include runtime stack sanity checks',
         'static':       'build static library',
         'no-shared':    'do not build shared library',
         'static-progs': 'build programs as static binaries',
         'largefile':    'build with large file support on 32-bit systems',
         'no-pcre':      'do not use PCRE, even if present',
         'no-posix':     'do not use POSIX functions, even if present'})


def configure(conf):
    conf.load('compiler_c', cache=True)

    if not Options.options.no_python:
        try:
            conf.load('python', cache=True)
            conf.check_python_version((3, 4, 0))
            conf.check_python_headers()
            conf.load('cython')
            conf.env.SERD_PYTHON = 1
        except Exception as e:
            Logs.warn('Failed to configure Python (%s)\n' % e)

    if not Options.options.no_cxx:
        conf.load('compiler_cxx', cache=True)

    conf.load('autowaf', cache=True)

    if not autowaf.set_c_lang(conf, 'c11', mandatory=False):
        autowaf.set_c_lang(conf, 'c99')

    if 'COMPILER_CXX' in conf.env:
        autowaf.set_cxx_lang(conf, 'c++11')

    if Options.options.strict:
        # Check for programs used by lint target
        conf.find_program("flake8", var="FLAKE8", mandatory=False)
        conf.find_program("clang-tidy", var="CLANG_TIDY", mandatory=False)
        conf.find_program("iwyu_tool", var="IWYU_TOOL", mandatory=False)

    if Options.options.ultra_strict:
        autowaf.add_compiler_flags(conf.env, '*', {
            'clang': [
                '-Wno-covered-switch-default',
                '-Wno-float-equal',
                '-Wno-format-nonliteral',
                '-Wno-implicit-fallthrough',
                '-Wno-nullability-extension',
                '-Wno-nullable-to-nonnull-conversion',
                '-Wno-padded',
                '-Wno-reserved-id-macro',
            ],
            'gcc': [
                '-Wno-float-equal',
                '-Wno-inline',
                '-Wno-padded',
            ],
            'msvc': [
                '/wd4061',  # enumerator in switch is not explicitly handled
                '/wd4200',  # nonstandard: zero-sized array in struct/union
                '/wd4464',  # relative include path contains '..'
                '/wd4514',  # unreferenced inline function has been removed
                '/wd4710',  # function not inlined
                '/wd4711',  # function selected for automatic inline expansion
                '/wd4820',  # padding added after construct
                '/wd4996',  # POSIX name for this item is deprecated
            ],
        })

        autowaf.add_compiler_flags(conf.env, 'c', {
            'clang': [
                '-Wno-bad-function-cast',
                '-Wno-cast-align',
            ],
            'gcc': [
                '-Wno-bad-function-cast',
                '-Wno-cast-align',
            ],
            'msvc': [
                '/wd4706',  # assignment within conditional expression
                '/wd5045',  # will insert Spectre mitigation for memory load
            ],
        })

        if 'mingw' in conf.env.CC[0]:
            conf.env.append_value('CFLAGS', [
                '-Wno-float-conversion',
                '-Wno-suggest-attribute=format',
                '-Wno-unused-macros',
            ])
            conf.env.append_value('CXXFLAGS', [
                '-Wno-suggest-attribute=format',
            ])

        autowaf.add_compiler_flags(conf.env, 'cxx', {
            'clang': [
                '-Wno-documentation-unknown-command',
            ],
            'gcc': [
                '-Wno-multiple-inheritance',
                '-Wno-suggest-attribute=pure',
            ],
            'msvc': [
                '/wd4355',  # 'this' used in base member initializer list
                '/wd4571',  # structured exceptions are no longer caught
                '/wd4623',  # default constructor implicitly deleted
                '/wd4625',  # copy constructor implicitly deleted
                '/wd4626',  # assignment operator implicitly deleted
                '/wd4710',  # function not inlined
                '/wd4868',  # may not enforce left-to-right evaluation order
                '/wd5026',  # move constructor implicitly deleted
                '/wd5027',  # move assignment operator implicitly deleted
            ]
        })

    conf.env.update({
        'BUILD_UTILS': not Options.options.no_utils,
        'BUILD_SHARED': not Options.options.no_shared,
        'STATIC_PROGS': Options.options.static_progs,
        'BUILD_STATIC': (Options.options.static or
                         Options.options.static_progs)})

    if not conf.env.BUILD_SHARED and not conf.env.BUILD_STATIC:
        conf.fatal('Neither a shared nor a static build requested')

    if Options.options.stack_check:
        conf.define('SERD_STACK_CHECK', SERD_VERSION)

    if Options.options.largefile:
        conf.env.append_unique('DEFINES', ['_FILE_OFFSET_BITS=64'])

    conf.check_function('c', 'aligned_alloc',
                        header_name = 'stdlib.h',
                        return_type = 'void*',
                        arg_types   = 'size_t,size_t',
                        define_name = 'HAVE_ALIGNED_ALLOC',
                        mandatory   = False)

    if not Options.options.no_posix:
        funcs = {'posix_memalign': ('stdlib.h', 'int', 'void**,size_t,size_t'),
                 'posix_fadvise':  ('fcntl.h', 'int', 'int,off_t,off_t,int'),
                 'fileno':         ('stdio.h', 'int', 'FILE*')}

        for name, (header, ret, args) in funcs.items():
            conf.check_function('c', name,
                                header_name = header,
                                return_type = ret,
                                arg_types   = args,
                                define_name = 'HAVE_' + name.upper(),
                                defines     = ['_POSIX_C_SOURCE=200809L'],
                                mandatory   = False)

    conf.check_cc(msg         = 'Checking for __builtin_clz',
                  define_name = 'HAVE_BUILTIN_CLZ',
                  fragment    = 'int main(void) {return __builtin_clz(0);}',
                  mandatory   = False)
    conf.check_cc(msg         = 'Checking for __builtin_clzll',
                  define_name = 'HAVE_BUILTIN_CLZLL',
                  fragment    = 'int main(void) {return __builtin_clzll(0);}',
                  mandatory   = False)

    if not Options.options.no_pcre:
        autowaf.check_pkg(conf, 'libpcre',
                          uselib_store='PCRE',
                          mandatory=False)

    if conf.env.HAVE_PCRE:
        if conf.check(cflags=['-pthread'], mandatory=False):
            conf.env.PTHREAD_CFLAGS = ['-pthread']
            if conf.env.CC_NAME != 'clang':
                conf.env.PTHREAD_LINKFLAGS = ['-pthread']
        elif conf.check(linkflags=['-lpthread'], mandatory=False):
            conf.env.PTHREAD_CFLAGS    = []
            conf.env.PTHREAD_LINKFLAGS = ['-lpthread']
        else:
            conf.env.PTHREAD_CFLAGS    = []
            conf.env.PTHREAD_LINKFLAGS = []

    # Set up environment for building/using as a subproject
    autowaf.set_lib_env(conf, 'serd', SERD_VERSION,
                        include_path=str(conf.path.find_node('include')))

    if conf.env.BUILD_TESTS:
        serdi_node = conf.path.get_bld().make_node('serdi_static')
    else:
        serdi_node = conf.path.get_bld().make_node('serdi')

    conf.env.SERDI = [serdi_node.abspath()]

    conf.write_config_header('serd_config.h', remove=False)

    autowaf.display_summary(
        conf,
        {'Build static library': bool(conf.env['BUILD_STATIC']),
         'Build shared library': bool(conf.env['BUILD_SHARED']),
         'Build utilities':      bool(conf.env['BUILD_UTILS']),
         'Build unit tests':     bool(conf.env['BUILD_TESTS']),
         'Python bindings':      bool(conf.env['SERD_PYTHON'])})


lib_headers = ['src/reader.h']

lib_source = ['src/base64.c',
              'src/bigint.c',
              'src/byte_sink.c',
              'src/byte_source.c',
              'src/cursor.c',
              'src/decimal.c',
              'src/env.c',
              'src/filter.c',
              'src/inserter.c',
              'src/int_math.c',
              'src/iter.c',
              'src/model.c',
              'src/n3.c',
              'src/node.c',
              'src/node_syntax.c',
              'src/nodes.c',
              'src/normalise.c',
              'src/range.c',
              'src/reader.c',
              'src/sink.c',
              'src/soft_float.c',
              'src/statement.c',
              'src/string.c',
              'src/syntax.c',
              'src/system.c',
              'src/uri.c',
              'src/validate.c',
              'src/world.c',
              'src/writer.c',
              'src/zix/btree.c',
              'src/zix/digest.c',
              'src/zix/hash.c']


def build(bld):
    # C Headers
    includedir = '${INCLUDEDIR}/serd-%s/serd' % SERD_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('include/serd/*.h'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'SERD', SERD_VERSION, SERD_MAJOR_VERSION, [],
                     {'SERD_MAJOR_VERSION': SERD_MAJOR_VERSION})

    if 'COMPILER_CXX' in bld.env:
        # C++ Headers
        includedirxx = '${INCLUDEDIR}/serdxx-%s/serd' % SERD_MAJOR_VERSION
        bld.install_files(
            includedirxx,
            bld.path.ant_glob('bindings/cxx/include/serd/*.hpp'))
        bld.install_files(
            includedirxx + '/detail',
            bld.path.ant_glob('bindings/cxx/include/serd/detail/*.hpp'))

        # C++ wrapper pkgconfig file
        autowaf.build_pc(bld, 'bindings/cxx/serdxx',
                         SERD_VERSION, SERD_MAJOR_VERSION, [],
                         {'SERD_MAJOR_VERSION': SERD_MAJOR_VERSION})

    defines = []
    lib_args = {'export_includes': ['include'],
                'includes':        ['.', 'include', './src'],
                'cflags':          ['-fvisibility=hidden'],
                'lib':             ['m'],
                'use':             ['PCRE'],
                'vnum':            SERD_VERSION,
                'install_path':    '${LIBDIR}'}
    if bld.env.MSVC_COMPILER:
        lib_args['cflags'] = []
        lib_args['lib']    = []
        defines            = []

    # Shared Library
    if bld.env.BUILD_SHARED:
        bld(features        = 'c cshlib',
            source          = lib_source,
            name            = 'libserd',
            target          = 'serd-%s' % SERD_MAJOR_VERSION,
            uselib          = 'PCRE',
            defines         = defines + ['SERD_SHARED', 'SERD_INTERNAL'],
            **lib_args)

    # Static library
    if bld.env.BUILD_STATIC:
        bld(features        = 'c cstlib',
            source          = lib_source,
            name            = 'libserd_static',
            target          = 'serd-%s' % SERD_MAJOR_VERSION,
            uselib          = 'PCRE',
            defines         = defines + ['SERD_INTERNAL'],
            **lib_args)

    # Python bindings
    if bld.env.SERD_PYTHON:
        cflags = [f for f in bld.env.CFLAGS if not f.startswith('-W')]
        linkflags = [f for f in bld.env.LINKFLAGS if f != '-Wl,--no-undefined']

        cython_env = bld.env.derive()
        cython_env.append_unique('CYTHONFLAGS', ['-Wextra', '-Werror'])
        cython_env['CFLAGS'] = cflags
        cython_env['LINKFLAGS'] = linkflags

        autowaf.add_compiler_flags(cython_env, 'c', {
            'gcc': [
                '-Wno-cast-align',
                '-Wno-inline',
                '-Wno-pedantic',
                '-Wno-redundant-decls',
                '-Wno-shadow',
                '-Wno-sign-conversion',
                '-Wno-suggest-attribute=format',
                '-Wno-suggest-attribute=pure',
                '-Wno-undef',
            ],
            'clang': [
                '-Wno-deprecated-declarations',
            ]
        })

        bld(features     = 'c cshlib pyext',
            source       = 'bindings/python/serd.pyx',
            target       = 'bindings/python/serd',
            includes     = '.',
            use          = 'libserd',
            install_path = '${PYTHONDIR}',
            env          = cython_env)

    if bld.env.BUILD_TESTS:
        coverage_flags = [''] if bld.env.NO_COVERAGE else ['--coverage']
        test_args = {'includes':     ['.', 'include', './src'],
                     'cflags':       coverage_flags,
                     'linkflags':    coverage_flags,
                     'lib':          lib_args['lib'],
                     'install_path': ''}

        # Profiled static library for test coverage
        bld(features     = 'c cstlib',
            source       = lib_source,
            name         = 'libserd_profiled',
            target       = 'serd_profiled',
            uselib       = 'PCRE',
            defines      = defines + ['SERD_INTERNAL'],
            **test_args)

        # Test programs
        for prog in [('serdi_static', 'src/serdi.c'),
                     ('test_base64', 'test/test_base64.c'),
                     ('test_bigint', 'test/test_bigint.c'),
                     ('test_cursor', 'test/test_cursor.c'),
                     ('test_decimal', 'test/test_decimal.c'),
                     ('test_env', 'test/test_env.c'),
                     ('test_free_null', 'test/test_free_null.c'),
                     ('test_int_math', 'test/test_int_math.c'),
                     ('test_model', 'test/test_model.c'),
                     ('test_node', 'test/test_node.c'),
                     ('test_node_syntax', 'test/test_node_syntax.c'),
                     ('test_nodes', 'test/test_nodes.c'),
                     ('test_overflow', 'test/test_overflow.c'),
                     ('test_read_chunk', 'test/test_read_chunk.c'),
                     ('test_reader_writer', 'test/test_reader_writer.c'),
                     ('test_sink', 'test/test_sink.c'),
                     ('test_soft_float', 'test/test_soft_float.c'),
                     ('test_statement', 'test/test_statement.c'),
                     ('test_string', 'test/test_string.c'),
                     ('test_terse_write', 'test/test_terse_write.c'),
                     ('test_uri', 'test/test_uri.c')]:
            bld(features     = 'c cprogram',
                source       = prog[1],
                use          = 'libserd_profiled',
                uselib       = 'PCRE',
                target       = prog[0],
                defines      = defines,
                **test_args)

        # C++ API test
        if 'COMPILER_CXX' in bld.env:
            cxx_test_args = test_args
            cxx_test_args['includes'] += ['bindings/cxx/include']
            cxx_test_args['cxxflags'] = coverage_flags
            bld(features     = 'cxx cxxprogram',
                source       = 'bindings/cxx/test/test_serd_cxx.cpp',
                use          = 'libserd_profiled',
                target       = 'test_serd_cxx',
                defines      = defines,
                **cxx_test_args)

        # Python API test
        if bld.env.SERD_PYTHON:
            # Copy test to build directory
            bld(features     = 'subst',
                is_copy      = True,
                source       = 'bindings/python/test_serd.py',
                target       = 'bindings/python/test_serd.py',
                install_path = None)

    # Utilities
    if bld.env.BUILD_UTILS:
        obj = bld(features     = 'c cprogram',
                  source       = 'src/serdi.c',
                  target       = 'serdi',
                  includes     = ['.', 'include', './src'],
                  use          = 'libserd',
                  uselib       = 'PCRE',
                  lib          = lib_args['lib'],
                  cflags       = bld.env.PTHREAD_CFLAGS,
                  linkflags    = bld.env.PTHREAD_LINKFLAGS,
                  install_path = '${BINDIR}')
        if not bld.env.BUILD_SHARED or bld.env.STATIC_PROGS:
            obj.use = 'libserd_static'
        if bld.env.STATIC_PROGS:
            obj.env.SHLIB_MARKER  = obj.env.STLIB_MARKER
            obj.linkflags        += ['-static']

    # Documentation
    if bld.env.DOCS:
        autowaf.build_dox(bld, 'SERD', SERD_VERSION, top, out)
        bld(features='subst',
            source='doc/index.html.in',
            target='index.html',
            install_path='',
            name='index',
            SERD_VERSION=SERD_VERSION)

    # Man page
    bld.install_files('${MANDIR}/man1', 'doc/serdi.1')

    bld.add_post_fun(autowaf.run_ldconfig)


class LintContext(Build.BuildContext):
    fun = cmd = 'lint'


def lint(ctx):
    "checks code for style issues"
    import subprocess

    st = 0

    if "FLAKE8" in ctx.env:
        Logs.info("Running flake8")
        st = subprocess.call([ctx.env.FLAKE8[0],
                              "wscript",
                              "--ignore",
                              "E101,E129,W191,E221,W504,E251,E241,E741"])
        st += subprocess.call([ctx.env.FLAKE8[0],
                               "scripts/serd_bench.py",
                               "--ignore",
                               "E203"])
    else:
        Logs.warn("Not running flake8")

    if "IWYU_TOOL" in ctx.env:
        Logs.info("Running include-what-you-use")
        cmd = [ctx.env.IWYU_TOOL[0], "-o", "clang", "-p", "build",
               "src", "test", "bindings/cxx/test"]
        output = subprocess.check_output(cmd).decode('utf-8')
        if 'error: ' in output:
            sys.stdout.write(output)
            st += 1
    else:
        Logs.warn("Not running include-what-you-use")

    if "CLANG_TIDY" in ctx.env and "clang" in ctx.env.CC[0]:
        Logs.info("Running clang-tidy")
        sources = glob.glob('include/serd/*.h*')
        sources = glob.glob('bindings/cxx/include/serd/*.hpp')
        sources = glob.glob('bindings/cxx/include/serd/detail/*.hpp')
        sources += glob.glob('src/*.c')
        sources += glob.glob('test/*.c*')
        sources += glob.glob('bindings/cxx/test/*.cpp')
        sources = list(map(os.path.abspath, sources))
        procs = []
        for source in sources:
            cmd = [ctx.env.CLANG_TIDY[0], "--quiet", "-p=.", source]
            procs += [subprocess.Popen(cmd, cwd="build")]

        for proc in procs:
            stdout, stderr = proc.communicate()
            st += proc.returncode
    else:
        Logs.warn("Not running clang-tidy")

    if st != 0:
        sys.exit(st)


def amalgamate(ctx):
    "builds single-file amalgamated source"
    import shutil
    import re
    shutil.copy('serd/serd.h', 'build/serd.h')

    def include_line(line):
        return (not re.match(r'#include "[^/]*\.h"', line) and
                not re.match('#include "serd/serd.h"', line))

    with open('build/serd.c', 'w') as amalgamation:
        amalgamation.write('/* This is amalgamated code, do not edit! */\n')
        amalgamation.write('#include "serd.h"\n\n')

        for header_path in ['src/namespaces.h',
                            'src/serd_internal.h',
                            'src/system.h',
                            'src/byte_sink.h',
                            'src/byte_source.h',
                            'src/stack.h',
                            'src/string_utils.h',
                            'src/uri_utils.h',
                            'src/reader.h']:
            with open(header_path) as header:
                for l in header:
                    if include_line(l):
                        amalgamation.write(l)

        for f in lib_headers + lib_source:
            with open(f) as fd:
                amalgamation.write('\n/**\n   @file %s\n*/' % f)
                for l in fd:
                    if include_line(l):
                        amalgamation.write(l)

    for i in ['c', 'h']:
        Logs.info('Wrote build/serd.%s' % i)


def earl_assertion(test, passed, asserter):
    import datetime

    asserter_str = ''
    if asserter is not None:
        asserter_str = '\n\tearl:assertedBy <%s> ;' % asserter

    return '''
[]
	a earl:Assertion ;%s
	earl:subject <http://drobilla.net/sw/serd> ;
	earl:test <%s> ;
	earl:result [
		a earl:TestResult ;
		earl:outcome %s ;
		dc:date "%s"^^xsd:dateTime
	] .
''' % (asserter_str,
       test,
       'earl:passed' if passed else 'earl:failed',
       datetime.datetime.now().replace(microsecond=0).isoformat())


serdi = './serdi_static'


def test_osyntax_options(osyntax):
    if osyntax.lower() == 'ntriples' or osyntax.lower() == 'nquads':
        return [['-a']]
    return []


def flatten_options(opts):
    return [o for sublist in opts for o in sublist]


def test_thru(check,
              base,
              path,
              check_path,
              flags,
              isyntax,
              osyntax,
              options=[]):
    out_path = path + '.pass'
    opts = options + flatten_options(test_osyntax_options(osyntax))
    flags = flatten_options(flags)
    osyntax_opts = [f for sublist in
                    test_osyntax_options(osyntax) for f in sublist]
    out_cmd = [serdi] + opts + flags + [
        '-i', isyntax,
        '-o', isyntax,
        '-p', 'foo',
        '-I', base,
        check.tst.src_path(path)]

    thru_path = path + '.thru'
    thru_cmd = [serdi] + opts + osyntax_opts + [
        '-i', isyntax,
        '-o', osyntax,
        '-c', 'foo',
        '-a',
        '-I', base,
        out_path]

    return (check(out_cmd, stdout=out_path, verbosity=0, name=out_path) and
            check(thru_cmd, stdout=thru_path, verbosity=0, name=thru_path) and
            check.file_equals(check_path, thru_path, verbosity=0))


def file_uri_to_path(uri):
    try:
        from urlparse import urlparse  # Python 2
    except ImportError:
        from urllib.parse import urlparse  # Python 3

    path  = urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    return path if not drive else path[1:]


def _test_output_syntax(test_class):
    if 'NTriples' in test_class or 'Turtle' in test_class:
        return 'NTriples'
    elif 'NQuads' in test_class or 'Trig' in test_class:
        return 'NQuads'
    raise Exception('Unknown test class <%s>' % test_class)


def _wrapped_command(cmd):
    if Options.options.wrapper:
        import shlex
        return shlex.split(Options.options.wrapper) + cmd

    return cmd


def _load_rdf(filename):
    "Load an RDF file into python dictionaries via serdi.  Only supports URIs."
    import subprocess
    import re

    rdf_type = 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type'
    model = {}
    instances = {}

    cmd = _wrapped_command(['./serdi_static', filename])
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    for line in proc.communicate()[0].splitlines():
        matches = re.match(r'<([^ ]*)> <([^ ]*)> <([^ ]*)> \.',
                           line.decode('utf-8'))
        if matches:
            s, p, o = (matches.group(1), matches.group(2), matches.group(3))
            if s not in model:
                model[s] = {p: [o]}
            elif p not in model[s]:
                model[s][p] = [o]
            else:
                model[s][p].append(o)

            if p == rdf_type:
                if o not in instances:
                    instances[o] = set([s])
                else:
                    instances[o].update([s])

    return model, instances


def _option_combinations(options):
    "Return an iterator that cycles through all combinations of options"
    import itertools

    combinations = []
    for n in range(len(options) + 1):
        combinations += list(itertools.combinations(options, n))

    return itertools.cycle(combinations)


def _file_lines_equal(patha, pathb, subst_from='', subst_to=''):
    import io

    for path in (patha, pathb):
        if not os.access(path, os.F_OK):
            Logs.pprint('RED', 'error: missing file %s' % path)
            return False

    la = sorted(set(io.open(patha, encoding='utf-8').readlines()))
    lb = sorted(set(io.open(pathb, encoding='utf-8').readlines()))
    if la != lb:
        autowaf.show_diff(la, lb, patha, pathb)
        return False

    return True


def test_suite(ctx,
               base_uri,
               testdir,
               report,
               isyntax,
               options=[],
               output_syntax=None):
    srcdir = ctx.path.abspath()

    mf = 'http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#'
    manifest_path = os.path.join(srcdir, 'test', testdir, 'manifest.ttl')
    model, instances = _load_rdf(manifest_path)

    asserter = ''
    if os.getenv('USER') == 'drobilla':
        asserter = 'http://drobilla.net/drobilla#me'

    def run_tests(test_class, tests, expected_return):
        thru_flags = [['-e'], ['-f'], ['-b'], ['-r', 'http://example.org/']]
        osyntax = output_syntax or _test_output_syntax(test_class)
        thru_options_iter = _option_combinations(thru_flags)
        tests_name = '%s.%s' % (testdir, test_class[test_class.find('#') + 1:])
        with ctx.group(tests_name) as check:
            for test in sorted(tests):
                action_node = model[test][mf + 'action'][0]
                basename    = os.path.basename(action_node)
                action      = os.path.join('test', testdir, basename)
                rel_action  = os.path.join(os.path.relpath(srcdir), action)
                uri         = base_uri + os.path.basename(action)
                command     = ([serdi, '-a', '-o', osyntax] +
                               ['-I', uri] +
                               options +
                               [rel_action])

                # Run strict test
                if expected_return == 0:
                    result = check(command,
                                   stdout=action + '.out',
                                   name=action)
                else:
                    result = check(command,
                                   stdout=action + '.out',
                                   stderr=autowaf.NONEMPTY,
                                   expected=expected_return,
                                   name=action)

                if (result and expected_return == 0 and
                    ((mf + 'result') in model[test])):
                    # Check output against test suite
                    check_uri  = model[test][mf + 'result'][0]
                    check_path = ctx.src_path(file_uri_to_path(check_uri))
                    result     = check.file_equals(action + '.out', check_path)

                    # Run round-trip tests
                    if result:
                        test_thru(check, uri, action, check_path,
                                  list(next(thru_options_iter)),
                                  isyntax, osyntax, options)

                # Write test report entry
                if report is not None:
                    report.write(earl_assertion(test, result, asserter))

                if expected_return == 0:
                    # Run model test for positive test (must succeed)
                    out_path = action + '.model.out'
                    check([command[0]] + ['-w', out_path, '-m'] + command[1:],
                          name=action + ' model')

                    if result and ((mf + 'result') in model[test]):
                        check(lambda: _file_lines_equal(check_path, out_path),
                              name=action + ' model check')

    ns_rdftest = 'http://www.w3.org/ns/rdftest#'
    for test_class, instances in instances.items():
        if test_class.startswith(ns_rdftest):
            expected = (1 if '-l' not in options and 'Negative' in test_class
                        else 0)
            run_tests(test_class, instances, expected)


def validation_test_suite(tst,
                          base_uri,
                          testdir,
                          isyntax,
                          osyntax,
                          options=''):
    srcdir = tst.path.abspath()
    schemas = glob.glob(os.path.join(srcdir, 'schemas', '*.ttl'))

    ns_serd    = 'http://drobilla.net/ns/serd#'
    mf         = 'http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#'
    mf_path    = os.path.join(srcdir, 'test', testdir, 'manifest.ttl')

    model, instances = _load_rdf(mf_path)
    for test_class, instances in instances.items():
        if not test_class.startswith(ns_serd):
            continue

        tests_name = 'validate.%s' % test_class[test_class.find('#') + 1:]
        with tst.group(tests_name) as check:
            expected = 1 if 'Negative' in test_class else 0
            for test in sorted(instances):
                action_node = model[test][mf + 'action'][0]
                name        = os.path.basename(action_node)
                action      = os.path.join('test', 'validate', name)
                rel_action  = os.path.join(os.path.relpath(srcdir), action)
                command     = (['./serdi_static', '-V', '-o', 'empty'] +
                               schemas + [rel_action])

                if tst.env.HAVE_PCRE or name != 'bad-literal-pattern.ttl':
                    check(command, expected=expected, name=action)


def test(tst):
    import subprocess
    import tempfile

    # Create test output directories
    for i in ['bad',
              'good',
              'lax',
              'normalise',
              'pattern',
              'terse',
              'multifile',
              'TurtleTests',
              'NTriplesTests',
              'NQuadsTests',
              'TriGTests']:
        try:
            test_dir = os.path.join('test', i)
            os.makedirs(test_dir)
            for i in glob.glob(test_dir + '/*.*'):
                os.remove(i)
        except Exception:
            pass

    srcdir = tst.path.abspath()

    if tst.env.SERD_PYTHON:
        with tst.group('python') as check:
            check([tst.env.PYTHON[0], '-m', 'unittest',
                   'discover', 'bindings/python'])

    with tst.group('Unit') as check:
        check(['./test_base64'])
        check(['./test_bigint'])
        check(['./test_cursor'])
        check(['./test_decimal'])
        check(['./test_env'])
        check(['./test_free_null'])
        check(['./test_int_math'])
        check(['./test_model'])
        check(['./test_node'])
        check(['./test_node_syntax'])
        check(['./test_nodes'])
        check(['./test_overflow'])
        check(['./test_read_chunk'])
        check(['./test_reader_writer'])
        check(['./test_sink'])
        check(['./test_soft_float'])
        check(['./test_statement'])
        check(['./test_string'])
        check(['./test_terse_write'])
        check(['./test_uri'])

        if 'COMPILER_CXX' in tst.env:
            check(['./test_serd_cxx'])

    def test_syntax_io(check, in_name, check_name, lang):
        in_path = 'test/good/%s' % in_name
        out_path = in_path + '.io'
        check_path = '%s/test/good/%s' % (srcdir, check_name)

        check([serdi, '-o', lang, '-I', in_path, '-w', out_path,
               '%s/%s' % (srcdir, in_path)],
              name=in_name)

        check.file_equals(check_path, out_path)

    with tst.group('ThroughSyntax') as check:
        test_syntax_io(check, 'base.ttl',       'base.ttl',        'turtle')
        test_syntax_io(check, 'qualify-in.ttl', 'qualify-out.ttl', 'turtle')
        test_syntax_io(check, 'pretty.trig',    'pretty.trig',     'trig')

    with tst.group('GoodCommands') as check:
        check([serdi, '%s/serd.ttl' % srcdir], stdout=os.devnull)
        check([serdi, '-v'])
        check([serdi, '-h'])
        check([serdi, '-k', '512', '-s', '<urn:eg:s> a <urn:eg:T> .'])
        check([serdi, os.devnull])

        with tempfile.TemporaryFile(mode='r') as stdin:
            check([serdi, '-'], stdin=stdin)

        with tempfile.TemporaryFile(mode='w') as stdout:
            cmd = [serdi, '-o', 'empty', '%s/serd.ttl' % srcdir]
            check(cmd, stdout=stdout)
            stdout.seek(0, 2)  # Seek to end
            check(lambda: stdout.tell() == 0, name='empty output')

    with tst.group('MultiFile') as check:
        path = '%s/test/multifile' % srcdir
        check([serdi, '-w', 'test/multifile/output.out.nq',
               '%s/input1.ttl' % path, '%s/input2.trig' % path])
        check.file_equals('%s/test/multifile/output.nq' % srcdir,
                          'test/multifile/output.out.nq')

    with tst.group('GrepCommand') as check:
        with tempfile.TemporaryFile(mode='w+') as stdout:
            cmd = _wrapped_command([
                serdi,
                '-g', '?s <urn:example:p> <urn:example:o> .',
                '-s',
                '<urn:example:s> <urn:example:p> <urn:example:o> .\n'
                '<urn:example:s> <urn:example:q> <urn:example:r> .\n'])
            check(lambda: subprocess.check_output(cmd).decode('utf-8') ==
                  '<urn:example:s> <urn:example:p> <urn:example:o> .\n',
                  name='wildcard subject')

    with tst.group('BadCommands',
                   expected=1,
                   stderr=autowaf.NONEMPTY) as check:
        check([serdi])
        check([serdi, '/no/such/file'])
        check([serdi, 'ftp://example.org/unsupported.ttl'])
        check([serdi, '-I'])
        check([serdi, '-c'])
        check([serdi, '-g'])
        check([serdi, '-i', 'illegal'])
        check([serdi, '-i', 'turtle'])
        check([serdi, '-i'])
        check([serdi, '-k'])
        check([serdi, '-k', '-1'])
        check([serdi, '-k', str(2**63 - 1)])
        check([serdi, '-k', '1024junk'])
        check([serdi, '-o', 'illegal'])
        check([serdi, '-o'])
        check([serdi, '-p'])
        check([serdi, '-q', '%s/test/bad/bad-base.ttl' % srcdir], stderr=None)
        check([serdi, '-r'])
        check([serdi, '-s'])
        check([serdi, '-z'])
        check([serdi, '-s', '<foo> a <Bar> .'])
        check([serdi] + ['%s/test/bad/bad-base.ttl' % srcdir] * 2)

    with tst.group('IoErrors', expected=1) as check:
        check([serdi, '-e', 'file://%s/' % srcdir], name='Read directory')
        check([serdi, 'file://%s/' % srcdir], name='Bulk read directory')
        if os.path.exists('/dev/full'):
            check([serdi, 'file://%s/test/good/manifest.ttl' % srcdir],
                  stdout='/dev/full', name='Write error')
            check([serdi, 'file://%s/test/good/manifest.ttl' % srcdir],
                  stdout='/dev/full', name='Long write error')
        if os.path.exists('/proc/cpuinfo'):
            check([serdi, '-w', '/proc/cpuinfo',
                   'file://%s/test/good/base.ttl' % srcdir],
                  name='Read-only write error')

    if sys.version_info.major >= 3:
        from waflib.extras import autoship
        try:
            with tst.group('NEWS') as check:
                news_path = os.path.join(srcdir, 'NEWS')
                entries = autoship.read_news(top=srcdir)
                autoship.write_news(entries, 'NEWS.norm')
                check.file_equals(news_path, 'NEWS.norm')

                meta_path = os.path.join(srcdir, 'serd.ttl')
                autoship.write_news(entries, 'NEWS.ttl',
                                    format='turtle', template=meta_path)

                ttl_entries = autoship.read_news('NEWS.ttl',
                                                 top=srcdir, format='turtle')

                autoship.write_news(ttl_entries, 'NEWS.round')
                check.file_equals(news_path, 'NEWS.round')
        except ImportError:
            Logs.warn('Failed to import rdflib, not running NEWS tests')

    # Serd-specific test suites
    serd_base = 'http://drobilla.net/sw/serd/test/'
    test_suite(tst, serd_base + 'good/', 'good', None, 'Turtle')
    test_suite(tst, serd_base + 'bad/', 'bad', None, 'Turtle')
    test_suite(tst, serd_base + 'lax/', 'lax', None, 'Turtle', ['-l'])
    test_suite(tst, serd_base + 'lax/', 'lax', None, 'Turtle')
    test_suite(tst, serd_base + 'pattern/', 'pattern', None, 'Turtle', ['-x'])
    test_suite(tst, serd_base + 'normalise/', 'normalise', None, 'Turtle',
               ['-n'])
    test_suite(tst, serd_base + 'terse/', 'terse', None, 'Turtle', ['-t'],
               output_syntax='Turtle')

    # Serd validation test suite
    with open('validation_earl.ttl', 'w') as report:
        serd_base = 'http://drobilla.net/sw/serd/test/'
        report.write('@prefix earl: <http://www.w3.org/ns/earl#> .\n'
                     '@prefix dc: <http://purl.org/dc/elements/1.1/> .\n')
        validation_test_suite(tst, serd_base + 'validate/', 'validate',
                              None, 'Turtle', 'NTriples')

    # Standard test suites
    with open('earl.ttl', 'w') as report:
        report.write('@prefix earl: <http://www.w3.org/ns/earl#> .\n'
                     '@prefix dc: <http://purl.org/dc/elements/1.1/> .\n')

        with open(os.path.join(srcdir, 'serd.ttl')) as serd_ttl:
            report.writelines(serd_ttl)

        w3c_base = 'http://www.w3.org/2013/'
        test_suite(tst, w3c_base + 'TurtleTests/',
                   'TurtleTests', report, 'Turtle')
        test_suite(tst, w3c_base + 'NTriplesTests/',
                   'NTriplesTests', report, 'NTriples')
        test_suite(tst, w3c_base + 'NQuadsTests/',
                   'NQuadsTests', report, 'NQuads')
        test_suite(tst, w3c_base + 'TriGTests/',
                   'TriGTests', report, 'Trig')
