import struct
from pathlib import Path


def inspect_pe(path):
    path = Path(path)
    data = path.read_bytes()
    if len(data) < 256 or data[:2] != b'MZ':
        raise ValueError('Not a Windows PE executable: ' + str(path))
    header = struct.unpack_from('<I', data, 60)[0]
    if header + 144 > len(data) or data[header:header + 4] != b'PE\0\0':
        raise ValueError('Invalid PE header: ' + str(path))
    machine, sections = struct.unpack_from('<HH', data, header + 4)
    optional = header + 24
    if machine != 0x8664 or struct.unpack_from('<H', data, optional)[0] != 0x20b:
        raise ValueError('Expected Windows x86-64 PE32+: ' + str(path))
    table = optional + struct.unpack_from('<H', data, header + 20)[0]
    if table + sections * 40 > len(data):
        raise ValueError('Truncated PE section table: ' + str(path))
    mappings = []
    for index in range(sections):
        entry = table + index * 40
        size, address, raw_size, raw = struct.unpack_from('<IIII', data, entry + 8)
        mappings.append((address, min(size, raw_size), raw))

    def offset(address):
        for start, size, raw in mappings:
            if start <= address < start + size:
                result = raw + address - start
                if result >= len(data):
                    break
                return result
        raise ValueError('Invalid PE import RVA: ' + str(path))

    imports = []
    address, size = struct.unpack_from('<II', data, optional + 120)
    if address:
        entry = offset(address)
        for _ in range(4096):
            if entry + 20 > len(data):
                raise ValueError('Truncated PE import table: ' + str(path))
            descriptor = struct.unpack_from('<IIIII', data, entry)
            if not any(descriptor):
                break
            start = offset(descriptor[3])
            end = data.find(b'\0', start, start + 1024)
            if end < 0:
                raise ValueError('Invalid PE import name: ' + str(path))
            imports.append(data[start:end].decode('ascii'))
            entry += 20
        else:
            raise ValueError('Unbounded PE imports: ' + str(path))
    return {'format': 'PE32+', 'architecture': 'x86_64', 'imports': imports}
