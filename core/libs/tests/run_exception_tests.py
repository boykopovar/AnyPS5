import argparse
import base64
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
RUNTIME = ROOT / 'core/libs/prx/libc/src/exception/Exports.cpp'
TEST = Path(__file__).with_name('ExceptionRuntime.cpp')
parser = argparse.ArgumentParser()
parser.add_argument('--prx', type=Path)
arguments = parser.parse_args()


def nid(symbol):
    digest = hashlib.sha1(symbol.encode() + bytes.fromhex('518d64a635ded8c1e6b039b1c3e55230')).digest()
    return base64.b64encode(digest[:8][::-1]).decode().rstrip('=').replace('/', '-')


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


with tempfile.TemporaryDirectory(prefix='anyps5-exceptions-') as directory:
    work = Path(directory)
    for optimization in ('-O0', '-O2'):
        runtime = work / 'runtime.o'
        test = work / 'test.o'
        executable = work / 'test'
        compiler = os.environ.get('CXX', 'c++')
        run(compiler, '-std=c++20', optimization, '-g', '-fPIC', '-c', RUNTIME, '-o', runtime)
        symbols = subprocess.check_output(['nm', '--defined-only', str(runtime)], text=True)
        exports = {line.split()[-1] for line in symbols.splitlines() if line.split()}
        unresolved = subprocess.check_output(['nm', '-u', str(runtime)], text=True)
        forbidden = ('__cxa_throw', '__cxa_begin_catch', '__cxa_end_catch', '__cxa_rethrow',
                     '__gxx_personality_v0', '_Unwind_Resume', '_Unwind_RaiseException', '_ZSt9terminatev')
        assert not any(line.split()[-1] in forbidden for line in unresolved.splitlines()), unresolved
        run(compiler, '-std=c++20', optimization, '-g', '-c', TEST, '-o', test)
        unresolved = subprocess.check_output(['nm', '-u', str(test)], text=True)
        mapping = []
        for line in unresolved.splitlines():
            symbol = line.split()[-1]
            if symbol + '_nid_postfix' in exports:
                mapping += ['--redefine-sym', symbol + '=' + symbol + '_nid_postfix']
        run('objcopy', *mapping, test)
        run(compiler, '-g', runtime, test, '-pthread', '-Wl,--eh-frame-hdr', '-o', executable)
        run(executable)
        print(compiler, optimization, 'standalone passed', flush=True)
        if arguments.prx:
            prx = arguments.prx.resolve()
            dynamic = subprocess.check_output(['readelf', '--dyn-syms', '--wide', str(prx)], text=True)
            prx_exports = {parts[7] for line in dynamic.splitlines()
                           if len(parts := line.split()) >= 8 and parts[6] != 'UND'}
            expected = {nid(symbol.removesuffix('_nid_postfix')) for symbol in exports if symbol.endswith('_nid_postfix')}
            assert expected <= prx_exports, sorted(expected - prx_exports)
            run(compiler, '-std=c++20', optimization, '-g', '-c', TEST, '-o', test)
            unresolved = subprocess.check_output(['nm', '-u', str(test)], text=True)
            mapping = []
            for line in unresolved.splitlines():
                symbol = line.split()[-1]
                if symbol in exports and symbol.endswith('_nid_postfix') or symbol + '_nid_postfix' in exports:
                    mapping += ['--redefine-sym', symbol + '=' + nid(symbol.removesuffix('_nid_postfix'))]
            run('objcopy', *mapping, test)
            run(compiler, '-g', test, prx, '-pthread', '-Wl,--eh-frame-hdr', '-Wl,-rpath,' + str(prx.parent), '-o', executable)
            run(executable)
            print(compiler, optimization, 'NID PRX passed', flush=True)
