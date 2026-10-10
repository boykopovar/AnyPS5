import re


def merge_constant_stores(ir):
    require_target = re.search(r'target triple = "(?:arm64|aarch64)-apple-', ir)
    if require_target is None:
        raise ValueError('constant-store reconstruction requires little-endian Apple ARM64 IR')
    if '__aps5_aot_bytes_' in ir:
        raise ValueError('constant-store reconstruction has already run')
    output, constants, pending = [], [], []
    pointers = {}
    groups = {}
    count = 0

    def flush():
        nonlocal count
        replacements = []
        removed = set()
        for root, stores in groups.items():
            memory = {}
            for line_index, offset, raw in stores:
                memory.update((offset + i, byte) for i, byte in enumerate(raw))
            addresses = sorted(memory)
            spans = []
            for address in addresses:
                if spans and spans[-1][1] == address:
                    spans[-1][1] += 1
                else:
                    spans.append([address, address + 1])
            for begin, end in spans:
                if end - begin < 16:
                    continue
                contributing = [(index, offset, raw) for index, offset, raw in stores if begin <= offset and offset + len(raw) <= end]
                if len(contributing) < 2:
                    continue
                raw = bytes(memory[address] for address in range(begin, end))
                name = f'__aps5_aot_bytes_{len(constants)}'
                value = ''.join(f'\\{byte:02X}' for byte in raw)
                constants.append(f'@{name} = private unnamed_addr constant [{len(raw)} x i8] c"{value}", align 16')
                replacements.extend([f'  %{name}_dest = getelementptr i8, ptr {root}, i64 {begin}',
                                     f'  call void @llvm.memcpy.p0.p0.i64(ptr %{name}_dest, ptr @{name}, i64 {len(raw)}, i1 false)'])
                removed.update(index for index, _, _ in contributing)
                count += 1
        output.extend(line for index, line in enumerate(pending) if index not in removed)
        output.extend(replacements)
        pending.clear()
        groups.clear()

    for line in ir.splitlines():
        allocation = re.match(r'\s*(%[-\w.]+) = alloca \[([0-9]+) x i8\],', line)
        gep = re.match(r'\s*(%[-\w.]+) = getelementptr(?: inbounds)?(?: nuw)? i8, ptr (%[-\w.]+), i64 (-?[0-9]+)$', line)
        store = re.fullmatch(r'\s*store i(8|16|32|64) (-?[0-9]+), ptr (%[-\w.]+), align [0-9]+', line)
        if allocation:
            flush()
            pointers[allocation[1]] = (allocation[1], 0, int(allocation[2]))
            output.append(line)
        elif gep and gep[2] in pointers:
            root, offset, size = pointers[gep[2]]
            pointers[gep[1]] = (root, offset + int(gep[3]), size)
            pending.append(line)
        elif store and store[3] in pointers:
            root, offset, size = pointers[store[3]]
            width = int(store[1]) // 8
            if not 0 <= offset <= size - width:
                flush()
                output.append(line)
                continue
            raw = (int(store[2]) & ((1 << (width * 8)) - 1)).to_bytes(width, 'little')
            groups.setdefault(root, []).append((len(pending), offset, raw))
            pending.append(line)
        else:
            flush()
            output.append(line)
            if line.startswith('define ') or line == '}':
                pointers.clear()
    flush()
    if count:
        if not re.search(r'^declare void @llvm.memcpy.p0.p0.i64\(', ir, re.MULTILINE):
            output.append('declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg)')
        output.extend(constants)
    return '\n'.join(output) + '\n', count
