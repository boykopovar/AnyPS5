import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def commands(root, relative, expected):
    source = (root / relative).read_text(encoding='utf-8')
    found = re.findall(r'target_compile_options\(([^\n)]*HostStandardStreams\.hpp[^\n)]*)\)', source)
    targets = [command.split()[0] for command in found]
    assert sorted(targets) == sorted(expected), (relative, targets)
    return ['target_compile_options(' + command + ')' for command in found]


def run(command):
    result = subprocess.run(command, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, (command, result.stdout, result.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--cmake', required=True)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--generator', required=True)
    parser.add_argument('--make-program', required=True)
    args = parser.parse_args()
    library_commands = commands(args.source_root, 'core/libs/CMakeLists.txt', ('libc', '${LIB_NAME}'))
    shader_commands = commands(args.source_root, 'core/shader/recompiler/CMakeLists.txt', ('${target}',))

    with tempfile.TemporaryDirectory(prefix='anyps5-forced-include-') as directory:
        source = Path(directory) / 'source with spaces'
        build = Path(directory) / 'build with spaces'
        libraries = source / 'core/libs'
        shaders = source / 'core/shader/recompiler'
        header = libraries / 'prx/libc/include/HostStandardStreams.hpp'
        header.parent.mkdir(parents=True)
        shaders.mkdir(parents=True)
        header.write_text('#define ANYPS5_FORCED_INCLUDE_TEST_VALUE 73\n', encoding='utf-8')
        check = 'static_assert(ANYPS5_FORCED_INCLUDE_TEST_VALUE == 73);\n'
        (libraries / 'probe.cpp').write_text(check, encoding='utf-8')
        (shaders / 'probe.cpp').write_text(check, encoding='utf-8')
        (source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.21)\n'
            'project(ForcedIncludePaths LANGUAGES CXX)\n'
            'set(CMAKE_CXX_STANDARD 20)\n'
            'add_subdirectory(core/libs)\n'
            'add_subdirectory(core/shader/recompiler)\n', encoding='utf-8')
        (libraries / 'CMakeLists.txt').write_text(
            'add_library(libc OBJECT probe.cpp)\n'
            'set(LIB_NAME library_loop)\n'
            'add_library(${LIB_NAME} OBJECT probe.cpp)\n'
            + '\n'.join(library_commands) + '\n', encoding='utf-8')
        (shaders / 'CMakeLists.txt').write_text(
            'set(target shader_recompiler)\n'
            'add_library(${target} OBJECT probe.cpp)\n'
            + '\n'.join(shader_commands) + '\n', encoding='utf-8')
        run([args.cmake, '-S', str(source), '-B', str(build), '-G', args.generator,
             '-DCMAKE_CXX_COMPILER=' + args.compiler,
             '-DCMAKE_MAKE_PROGRAM=' + args.make_program])
        failures = []
        for target in ('libc', 'library_loop', 'shader_recompiler'):
            try:
                run([args.cmake, '--build', str(build), '--config', 'Release', '--target', target])
            except AssertionError as error:
                failures.append((target, str(error)))
            else:
                print(target + ': forced header compiled from a path with spaces')
        assert not failures, failures


if __name__ == '__main__':
    main()
