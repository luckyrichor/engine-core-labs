"""Pinned, user-local source builds. Invoke through build.sh; no system install."""
import datetime
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
DATA = Path(os.environ.get('ECL_DATA', Path.home()/'data/engine-core-labs')).resolve()
BASE = DATA/'third_party'
JOBS = os.environ.get('ECL_BUILD_JOBS', '2')
REPOS = {'oneTBB': 'uxlfoundation/oneTBB', 'Taskflow': 'taskflow/taskflow',
         'jemalloc': 'jemalloc/jemalloc', 'mimalloc': 'microsoft/mimalloc',
         'gperftools': 'gperftools/gperftools', 'google-tcmalloc': 'google/tcmalloc'}
ROWS = ['oneTBB', 'Taskflow', 'jemalloc', 'mimalloc', 'tcmalloc（gperftools）', 'tcmalloc（google/tcmalloc）']

def run(*args, cwd=None):
    print('+', shlex.join(map(str,args)), flush=True)
    subprocess.run(list(map(str,args)), cwd=cwd, check=True)

def output(*args):
    return subprocess.check_output(list(map(str,args)), text=True).strip()

def pin(name):
    row = ROWS[list(REPOS).index(name)]
    line = next(s for s in (ROOT/'third_party/versions.md').read_text().splitlines() if s.startswith('| '+row+' |'))
    cells = [s.strip() for s in line.split('|')]
    return cells[3], cells[4]

def build(name):
    tag, commit = pin(name)
    label = commit if tag == '无标签' else tag
    source = BASE/'src'/name
    source.parent.mkdir(parents=True, exist_ok=True)
    # Reuse the verified Google checkout and its existing CMake build/dependency cache.
    if name == 'google-tcmalloc' and not source.exists() and (BASE/'tcmalloc-src').exists():
        source.symlink_to(BASE/'tcmalloc-src', target_is_directory=True)
    if not source.exists():
        run('git', 'clone', '--filter=blob:none', '--no-checkout', 'git@github.com:'+REPOS[name]+'.git', source)
        run('git', '-C', source, 'checkout', '--detach', commit if tag == '无标签' else tag)
    actual = output('git', '-C', source, 'rev-parse', 'HEAD')
    if not actual.startswith(commit):
        raise RuntimeError(f'{name}: commit mismatch: expected {commit}, got {actual}')
    if output('git', '-C', source, 'diff', 'HEAD', '--'):
        raise RuntimeError(f'{name}: tracked source modifications; refusing build')
    prefix = BASE/'install'/f'{name}-{label}'
    prefix.mkdir(parents=True, exist_ok=True)
    info_path = prefix/'build-info.json'
    # An old receipt must never certify a failed rebuild.
    info_path.unlink(missing_ok=True)
    build_dir = BASE/'build'/f'{name}-{label}'
    options = []
    flags = 'upstream Release default'
    if name in {'oneTBB', 'Taskflow', 'mimalloc'}:
        options = {'oneTBB': ['-DTBB_TEST=OFF'],
                   'Taskflow': ['-DTF_BUILD_TESTS=OFF', '-DTF_BUILD_EXAMPLES=OFF', '-DTF_BUILD_BENCHMARKS=OFF'],
                   'mimalloc': ['-DMI_BUILD_TESTS=OFF']}[name]
        run('cmake', '-S', source, '-B', build_dir, '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_INSTALL_LIBDIR=lib', f'-DCMAKE_INSTALL_PREFIX={prefix}', *options)
        run('cmake', '--build', build_dir, '-j', JOBS)
        run('cmake', '--install', build_dir)
        flags = output('cmake', '-LA', '-N', build_dir)
        flags = {s.split(':',1)[0]: s.split('=',1)[1] for s in flags.splitlines()
                 if s.startswith(('CMAKE_CXX_FLAGS_RELEASE:', 'CMAKE_C_FLAGS_RELEASE:'))}
    elif name in {'jemalloc', 'gperftools'}:
        if not all(shutil.which(tool) for tool in ('autoconf', 'automake', 'libtoolize', 'm4')):
            if sys.platform != 'linux' or not shutil.which('apt-get'):
                raise RuntimeError('autoconf/automake/libtool/m4 required; see docs/baselines.md')
            from autotools import prepare
            prepare(DATA)
        build_dir.mkdir(parents=True, exist_ok=True)
        if name == 'jemalloc':
            if not (source/'configure').exists():
                run('autoconf', cwd=source)
            options = ['--disable-initial-exec-tls', f'--with-version={tag}-0-g{actual}']
        else:
            if not (source/'Makefile.in').exists():
                run('autoreconf', '-fi', cwd=source)
            options = ['--disable-libunwind', '--disable-dependency-tracking']
        flags = '-O2 -DNDEBUG'
        configure_args = [str(source/'configure'), f'--prefix={prefix}', *options,
                          'CFLAGS='+flags, 'CXXFLAGS='+flags]
        config_receipt = build_dir/'ecl-configure.json'
        if (not (build_dir/'config.status').exists() or not config_receipt.exists()
                or json.loads(config_receipt.read_text()) != configure_args):
            run(*configure_args, cwd=build_dir)
            config_receipt.write_text(json.dumps(configure_args)+chr(10))
        run('make', '-j', JOBS, cwd=build_dir)
        run('make', 'install', cwd=build_dir)
    else:
        if sys.platform != 'linux':
            raise RuntimeError('google-tcmalloc is a Linux-only baseline')
        build_dir = BASE/'tcmalloc-cmake'
        if not (build_dir/'CMakeCache.txt').exists():
            raise RuntimeError('required stage-1 tcmalloc-cmake cache missing')
        # A configure-only consumer lets CMake resolve the exact transitive link
        # closure. It is NEVER built (in particular no upstream test is built).
        probe = BASE/'build/google-export'
        probe.mkdir(parents=True, exist_ok=True)
        (probe/'main.cpp').write_text('int main() { return 0; }\n')
        (probe/'export.cmake').write_text('''function(ecl_export_google)
  add_executable(ecl_link_probe EXCLUDE_FROM_ALL "${ECL_EXPORT_DIR}/main.cpp")
  target_link_libraries(ecl_link_probe PRIVATE tcmalloc::tcmalloc)
  file(GENERATE OUTPUT "${ECL_EXPORT_DIR}/includes.txt" CONTENT "$<JOIN:$<TARGET_PROPERTY:ecl_link_probe,INCLUDE_DIRECTORIES>,\\n>")
endfunction()
cmake_language(DEFER CALL ecl_export_google)
''')
        options = ['-DFETCHCONTENT_FULLY_DISCONNECTED=ON', f'-DECL_EXPORT_DIR={probe}',
                   f'-DCMAKE_PROJECT_tcmalloc_INCLUDE={probe}/export.cmake']
        run('cmake', '-S', source.resolve(), '-B', build_dir, '-DCMAKE_BUILD_TYPE=Release', *options)
        run('cmake', '--build', build_dir, '--target', 'tcmalloc_tcmalloc', '-j', JOBS)
        tokens = shlex.split((build_dir/'CMakeFiles/ecl_link_probe.dir/link.txt').read_text())
        links = []
        for token in tokens:
            if token.endswith('.a'):
                src = Path(token)
                if not src.is_absolute():
                    src = build_dir/src
                rel = src.relative_to(build_dir)
                dest = prefix/'lib'/rel
                dest.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(src,dest)
                links.append(str(dest))
            elif token.startswith(('-Wl,', '-l', '-pthread')):
                links.append(token)
        if not any('libtcmalloc_tcmalloc.a' in s for s in links):
            raise RuntimeError('google transitive link export missing allocator')
        includes = []
        for i, inc in enumerate(dict.fromkeys((probe/'includes.txt').read_text().splitlines())):
            incpath = Path(inc)
            if not incpath.is_dir():
                continue
            dst = prefix/'include'/str(i)
            # Only public header trees, no source/build artifacts.
            for header in incpath.rglob('*'):
                if not header.is_file() or header.suffix not in {'.h', '.inc', '.def'}:
                    continue
                if any(part in {'.git', 'dist', 'bazel-tcmalloc-src'} for part in header.relative_to(incpath).parts):
                    continue
                target = dst/header.relative_to(incpath)
                target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(header,target)
            includes.append(str(dst))
        (prefix/'google-tcmalloc.cmake').write_text(
            'add_library(ecl_google_tcmalloc INTERFACE)\n'
            'target_include_directories(ecl_google_tcmalloc INTERFACE '+ ' '.join('"'+s+'"' for s in includes)+')\n'
            'target_link_libraries(ecl_google_tcmalloc INTERFACE '+ ' '.join('"'+s+'"' for s in links)+')\n')
        flags = next(s.split('=',1)[1] for s in output('cmake','-LA','-N',build_dir).splitlines() if s.startswith('CMAKE_CXX_FLAGS_RELEASE:'))
    deps = {}
    build_tools = {}
    if (DATA/'build-tools/receipt.json').exists():
        build_tools = json.loads((DATA/'build-tools/receipt.json').read_text())
    if name == 'google-tcmalloc':
        for dep in sorted((build_dir/'_deps').glob('*-src')):
            if (dep/'.git').exists():
                deps[dep.name] = output('git','-C',dep,'rev-parse','HEAD')
    info = dict(name=name, tag=tag, commit=actual, build_type='Release',
                release_flags=flags, options=options, source=str(source.resolve()),
                build=str(build_dir), install=str(prefix), jobs=int(JOBS),
                compiler=output('cc' if name in {'jemalloc', 'mimalloc'} else 'c++','--version').splitlines()[0],
                dependencies=deps, build_tools=build_tools, built_at=datetime.datetime.now(datetime.timezone.utc).isoformat())
    info_path.write_text(json.dumps(info,indent=2)+'\n')
    print('BUILD_OK',name,info_path,flush=True)

if __name__ == '__main__':
    if len(sys.argv)!=2 or sys.argv[1] not in {*REPOS, 'all'}:
        sys.exit('usage: build.sh <oneTBB|Taskflow|jemalloc|mimalloc|gperftools|google-tcmalloc|all>')
    for library in REPOS if sys.argv[1]=='all' else [sys.argv[1]]:
        build(library)
