from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


SOURCE = Path(sys.argv.pop(1))
CMAKE = sys.argv.pop(1)


class Fixture:
    def __init__(self, root, spirv_tools=False):
        self.root = root
        self.recompiler = root / 'core/shader/recompiler'
        self.build = root / 'build'
        self.calls = 0
        for directory, directories, files in os.walk(SOURCE, followlinks=False):
            for path in (Path(directory), *(Path(directory) / name for name in directories + files)):
                if path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & 0x400:
                    raise AssertionError('The source fixture contains a reparse point')
        shutil.copytree(SOURCE, self.recompiler)
        header = root / 'core/libs/prx/libc/include/HostThreadLocal.hpp'
        header.parent.mkdir(parents=True)
        header.write_text('struct FixtureThreadLocal {};\n', encoding='utf-8')
        self.guard = root / 'driver.cpp'
        self.guard.write_text('int fixture_driver;\n', encoding='utf-8')
        compiler = root / ('compiler.cmd' if os.name == 'nt' else 'compiler')
        if os.name == 'nt':
            compiler.write_text('@echo off\nif "%~1"=="-print-sysroot" if "%~2"=="" exit /b 0\nif "%~1"=="--version" if "%~2"=="" exit /b 0\n>>"%~dp0compiler-invoked" echo %*\nexit /b 99\n', encoding='utf-8')
        else:
            compiler.write_text('#!/bin/sh\nif [ "$#" -eq 1 ]; then case "$1" in -print-sysroot|--version) exit 0;; esac; fi\nprintf "%s\\n" "$*" >> "$(dirname "$0")/compiler-invoked"\nexit 99\n', encoding='utf-8')
            compiler.chmod(0o755)
        mode = 'ON' if spirv_tools else 'OFF'
        (root / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_COMPILER "{compiler.as_posix()}")
set(CMAKE_CXX_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_VERSION 15.2.0)
set(CMAKE_CXX_COMPILER_ID_RUN TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
set(CMAKE_CXX_ABI_COMPILED TRUE)
set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT 17)
set(CMAKE_CXX_EXTENSIONS_COMPUTED_DEFAULT ON)
set(CMAKE_CXX_COMPILE_FEATURES cxx_std_20)
project(ShaderCacheSourceFixture LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Release)
set(ANYPS5_ENABLE_SPIRV_TOOLS {mode})
set(ANYPS5_USE_SYSTEM_DEPS ON)
set(ANYPS5_SYSTEM_DEPS_STAMP fixture)
add_library(fixture_runtime INTERFACE)
add_library(glslang INTERFACE)
add_library(SPIRV INTERFACE)
add_library(SPIRV-Tools-opt INTERFACE)
add_library(SPIRV-Tools-static INTERFACE)
include("${{CMAKE_CURRENT_SOURCE_DIR}}/core/shader/recompiler/CMakeLists.txt")
add_shader_recompiler(shader_recompiler_probe fixture_runtime GUARD_SOURCES "${{CMAKE_CURRENT_SOURCE_DIR}}/driver.cpp")
get_target_property(runtimeSources shader_recompiler_probe SOURCES)
list(JOIN runtimeSources "\\n" runtimeSources)
file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/runtime-sources.txt" "${{runtimeSources}}\\n")
''', encoding='utf-8')
        self.configure()

    def configure(self, flags=''):
        result = subprocess.run([CMAKE, '-S', str(self.root), '-B', str(self.build), '-G', 'Ninja', f'-DCMAKE_CXX_FLAGS={flags}'],
                                capture_output=True, encoding='utf-8', errors='replace', timeout=15)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        if (self.root / 'compiler-invoked').exists():
            raise AssertionError('The generator fixture invoked a compiler: ' + (self.root / 'compiler-invoked').read_text(encoding='utf-8'))
        self.generated = self.build / 'shader_recompiler_probe_generated'
        self.listing = self.generated / 'sources.txt'
        ninja = (self.build / 'build.ninja').read_text(encoding='utf-8')
        command = next(line for line in ninja.splitlines() if line.startswith('  COMMAND = ') and 'ShaderCacheVersion.cmake' in line)
        self.extra = re.search(r'"-DEXTRA=([^"]*)"', command).group(1)
        self.compiled = []
        for line in ninja.splitlines():
            match = re.match(r'build .*: CXX_COMPILER__shader_recompiler_probe_[^ ]+ (.*?) \|\|', line)
            if match:
                self.compiled.append(Path(match.group(1).replace('$:', ':').replace('$ ', ' ')))

    def version(self, success=True):
        self.calls += 1
        output = self.build / f'probe-version-{self.calls}.cpp'
        result = subprocess.run([CMAKE, f'-DROOT={self.root.as_posix()}',
                                 f'-DRECOMPILER_DIR={self.recompiler.as_posix()}',
                                 f'-DLIST_FILE={self.listing.as_posix()}', f'-DOUTPUT={output.as_posix()}',
                                 f'-DEXTRA={self.extra}', '-P', str(self.recompiler / 'ShaderCacheVersion.cmake')],
                                capture_output=True, encoding='utf-8', errors='replace', timeout=10)
        if not success:
            if result.returncode == 0 or output.exists():
                raise AssertionError('The generator accepted an unkeyed dependency')
            return result.stdout + result.stderr
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return re.search(r'SourceVersion = 0x([0-9a-f]+)ull', output.read_text(encoding='utf-8')).group(1)

    def sources(self):
        return {Path(line.split(' ', 1)[1]) for line in self.listing.read_text(encoding='utf-8').splitlines() if line.startswith('sources ')}


class ShaderCacheRuntimeSourceTests(unittest.TestCase):
    def fixture(self, spirv_tools=False):
        temporary = tempfile.TemporaryDirectory(prefix='shader_cache_sources_')
        self.addCleanup(temporary.cleanup)
        return Fixture(Path(temporary.name), spirv_tools)

    def test_runtime_source_graph_off_and_on(self):
        for enabled in (False, True):
            with self.subTest(spirv_tools=enabled):
                fixture = self.fixture(enabled)
                generated = fixture.generated / 'ShaderCacheVersion.cpp'
                selected = {Path(line) for line in (fixture.build / 'runtime-sources.txt').read_text(encoding='utf-8').splitlines()}
                self.assertEqual(set(fixture.compiled), selected)
                self.assertNotIn(generated, fixture.sources())
                self.assertEqual({path for path in fixture.sources() if path.suffix in ('.cpp', '.cc', '.cxx')}, selected - {generated})
                self.assertIn(fixture.recompiler / 'tests/DummyShaders.cpp', fixture.sources())
                self.assertEqual(fixture.recompiler / 'SpirvBackend/src/SpirvOptimizer.cpp' in fixture.sources(), enabled)
                fixture.version()

    def test_uncompiled_test_sources_do_not_change_version(self):
        fixture = self.fixture()
        previous = fixture.version()
        for name in ('CacheFileReadTests.cpp', 'RequestSerializerTests.cpp'):
            path = fixture.recompiler / 'tests' / name
            self.assertNotIn(path, fixture.sources())
            path.write_bytes(path.read_bytes() + b'\n')
            self.assertEqual(fixture.version(), previous, name)
        (fixture.recompiler / 'tests/NewFixture.cpp').write_text('int new_fixture;\n', encoding='utf-8')
        fixture.configure()
        self.assertEqual(fixture.version(), previous)

    def test_runtime_sources_headers_and_cmake_change_version(self):
        fixture = self.fixture()
        previous = fixture.version()
        for name in ('Recompiler.cpp', 'tests/DummyShaders.cpp', 'tests/DummyShaders.hpp', 'BdaAbi.hpp', 'CMakeLists.txt', 'ShaderCacheVersion.cmake'):
            with self.subTest(source=name):
                path = fixture.recompiler / name
                before = path.read_bytes()
                path.write_bytes(before + b'\n')
                self.assertNotEqual(fixture.version(), previous)
                path.write_bytes(before)
                self.assertEqual(fixture.version(), previous)

    def test_flags_change_version(self):
        fixture = self.fixture()
        previous = fixture.version()
        fixture.configure('-DSHADER_CACHE_FIXTURE=1')
        self.assertNotEqual(fixture.version(), previous)

    def test_optimizer_content_only_changes_enabled_version(self):
        for enabled in (False, True):
            with self.subTest(spirv_tools=enabled):
                fixture = self.fixture(enabled)
                previous = fixture.version()
                optimizer = fixture.recompiler / 'SpirvBackend/src/SpirvOptimizer.cpp'
                optimizer.write_bytes(optimizer.read_bytes() + b'\n')
                self.assertEqual(fixture.version() == previous, not enabled)

    def test_uncompiled_internal_include_is_rejected(self):
        fixture = self.fixture()
        source = fixture.recompiler / 'Recompiler.cpp'
        source.write_text('#include "tests/CacheFileReadTests.cpp"\n' + source.read_text(encoding='utf-8'), encoding='utf-8')
        self.assertIn('does not hash', fixture.version(False))

    def test_unhashed_external_include_is_rejected(self):
        fixture = self.fixture()
        (fixture.root / 'external.hpp').write_text('int external_fixture;\n', encoding='utf-8')
        source = fixture.recompiler / 'Recompiler.cpp'
        source.write_text('#include "../../../external.hpp"\n' + source.read_text(encoding='utf-8'), encoding='utf-8')
        self.assertIn('does not hash', fixture.version(False))

    def test_unreviewed_driver_call_is_rejected(self):
        fixture = self.fixture()
        fixture.guard.write_text('ShaderRecompiler::UnreviewedFixtureCall();\n', encoding='utf-8')
        self.assertIn('has not been reviewed', fixture.version(False))


unittest.main()
