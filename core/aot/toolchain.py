import re
import struct
import subprocess
import tempfile
from pathlib import Path

from optimize import merge_constant_stores


def build_native_object(ir, destination):
    destination = Path(destination)
    with tempfile.TemporaryDirectory(prefix='anyps5-native-build-') as directory:
        folder = Path(directory)
        source, optimized, merged, object_file = (folder / name for name in ('input.ll', 'optimized.ll', 'merged.ll', 'native.o'))
        source.write_text(ir)
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-S', '-emit-llvm', str(source), '-o', str(optimized)],
                       check=True, capture_output=True, text=True)
        result, count = merge_constant_stores(optimized.read_text())
        if re.search(r'%(?:rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp|r[8-9]|r1[0-5]|(?:xmm|ymm_hi)(?:[0-9]|1[0-5])|[czsop]f)(?:\.\w+)?\s*=\s*alloca', result) or 'freeze ' in result:
            raise ValueError('native lowering left unresolved machine-state values')
        if re.search(r'^\s*ret\b.*\b(?:undef|poison)\b', result, re.MULTILINE):
            raise ValueError('native lowering produced an undefined return value')
        merged.write_text(result)
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-c', str(merged), '-o', str(object_file)],
                       check=True, capture_output=True, text=True)
        raw = object_file.read_bytes()
        if len(raw) < 32 or struct.unpack_from('<III', raw)[:2] != (0xfeedfacf, 0x100000c) or struct.unpack_from('<I', raw, 12)[0] != 1:
            raise ValueError('compiler did not produce an ARM64 Mach-O object')
        destination.write_bytes(raw)
        destination.with_suffix('.optimized.ll').write_text(result)
        return {'object_bytes': len(raw), 'constant_byte_runs_reconstructed': count, 'machine_state_allocas': 0,
                'status': 'native_object_not_product_qualified'}
