from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG


MOVES = {'vmovups', 'vmovaps', 'vmovdqa', 'vmovdqu'}
BITWISE = {'vpand': 'and', 'vpor': 'or', 'vpxor': 'xor', 'vxorps': 'xor'}
SUPPORTED = MOVES | set(BITWISE) | {'vmovd', 'vmovq', 'vpbroadcastd', 'vpshufd', 'vpsllvd', 'vzeroupper'}
MASK128 = (1 << 128) - 1


def _register(ins, operand):
    if operand.type != X86_OP_REG:
        return None
    name = ins.reg_name(operand.reg)
    if name.startswith(('xmm', 'ymm')) and name[3:].isdigit() and int(name[3:]) < 16:
        return 'xmm' + name[3:], 'ymm_hi' + name[3:], 128 if name.startswith('xmm') else 256
    return None


def _known(function, ins, state, operand, bits):
    register = _register(ins, operand)
    if register is not None:
        low, high, width = register
        if bits > width:
            function.fail(ins, 'AVX operand width exceeds its register')
        known = state['known'].get(low, 0)
        if bits > 128:
            known |= state['known'].get(high, 0) << 128
        return known & ((1 << bits) - 1)
    if operand.type == X86_OP_MEM:
        function.read_operand(ins, state, operand)
    elif operand.type == X86_OP_REG:
        function.read_operand(ins, state, operand)
    else:
        function.fail(ins, 'invalid AVX source')
    return (1 << bits) - 1


def _require(function, ins, state, operand, bits):
    if _known(function, ins, state, operand, bits) != (1 << bits) - 1:
        function.fail(ins, f'AVX source has undefined bits in its low {bits} bits')


def _define(function, ins, state, operand, known, bits):
    register = _register(ins, operand)
    if register is None:
        function.define(ins, state, operand)
        return
    low, high, width = register
    if bits > width:
        function.fail(ins, 'AVX result width exceeds its register')
    low_mask = (1 << min(bits, 128)) - 1
    state['known'][low] = (known & low_mask) | (MASK128 ^ low_mask)
    state['known'][high] = (known >> 128) & MASK128 if bits == 256 else MASK128


def _same(left, right):
    return left.type == right.type == X86_OP_REG and left.reg == right.reg


def analyze(function, ins, state):
    op, args = ins.mnemonic, ins.operands
    if op not in SUPPORTED:
        return False
    if ins.bytes[0] not in (0xc4, 0xc5):
        function.fail(ins, 'only VEX encodings are implemented; EVEX masking requires separate lowering')
    if op == 'vzeroupper':
        for index in range(16):
            state['known'][f'ymm_hi{index}'] = MASK128
        return True
    bits = args[0].size * 8
    if op in MOVES:
        if _register(ins, args[0]):
            known = _known(function, ins, state, args[1], bits)
        else:
            _require(function, ins, state, args[1], bits)
            known = (1 << bits) - 1
        _define(function, ins, state, args[0], known, bits)
    elif op in ('vmovd', 'vmovq'):
        width = 32 if op == 'vmovd' else 64
        if _register(ins, args[0]):
            known = _known(function, ins, state, args[1], width)
        else:
            _require(function, ins, state, args[1], width)
            known = (1 << width) - 1
        _define(function, ins, state, args[0], known, width)
    elif op in BITWISE:
        zero = BITWISE[op] == 'xor' and _same(args[1], args[2])
        known = (1 << bits) - 1 if zero else _known(function, ins, state, args[1], bits) & _known(function, ins, state, args[2], bits)
        _define(function, ins, state, args[0], known, bits)
    elif op == 'vpbroadcastd':
        scalar = _known(function, ins, state, args[1], 32)
        known = sum(scalar << offset for offset in range(0, bits, 32))
        _define(function, ins, state, args[0], known, bits)
    elif op == 'vpshufd':
        if args[2].type != X86_OP_IMM:
            function.fail(ins, 'AVX dword shuffle requires an immediate control')
        source = _known(function, ins, state, args[1], bits)
        known = 0
        for lane in range(bits // 32):
            selected = (lane // 4) * 4 + ((args[2].imm >> ((lane % 4) * 2)) & 3)
            known |= ((source >> (selected * 32)) & 0xffffffff) << (lane * 32)
        _define(function, ins, state, args[0], known, bits)
    elif op == 'vpsllvd':
        both = _known(function, ins, state, args[1], bits) & _known(function, ins, state, args[2], bits)
        known = 0
        for offset in range(0, bits, 32):
            if (both >> offset) & 0xffffffff == 0xffffffff:
                known |= 0xffffffff << offset
        _define(function, ins, state, args[0], known, bits)
    return True


def _read(function, ins, operand, bits):
    register = _register(ins, operand)
    if register is None:
        return function.read(ins, operand, bits)
    low, high, _ = register
    value = function.load(low, min(bits, 128))
    if bits <= 128:
        return value
    value = function.emit(f'zext i128 {value} to i256')
    upper = function.emit(f'zext i128 {function.load(high, 128)} to i256')
    upper = function.emit(f'shl i256 {upper}, 128')
    return function.emit(f'or i256 {value}, {upper}')


def _write(function, ins, operand, value, bits):
    register = _register(ins, operand)
    if register is None:
        function.write(ins, operand, value)
        return
    low, high, _ = register
    if bits == 256:
        low_value = function.emit(f'trunc i256 {value} to i128')
        high_value = function.emit(f'lshr i256 {value}, 128')
        high_value = function.emit(f'trunc i256 {high_value} to i128')
        function.store_vector(low, low_value)
        function.store_vector(high, high_value)
    else:
        function.store_vector(low, value, bits, zero_upper=True)
        function.store_vector(high, '0')


def _aligned(function, ins, bits):
    for operand in ins.operands:
        if operand.type != X86_OP_MEM:
            continue
        pointer = function.address(ins, operand)
        address = function.emit(f'ptrtoint ptr {pointer} to i64')
        residue = function.emit(f'and i64 {address}, {bits // 8 - 1}')
        aligned = function.emit(f'icmp eq i64 {residue}, 0')
        label = aligned[1:]
        function.lines.extend([f'  br i1 {aligned}, label %{label}_aligned, label %{label}_misaligned',
                               f'{label}_misaligned:', '  call void @llvm.trap()', '  unreachable', f'{label}_aligned:'])


def lower(function, ins):
    op, args = ins.mnemonic, ins.operands
    if op not in SUPPORTED:
        return False
    if op == 'vzeroupper':
        for index in range(16):
            function.store_vector(f'ymm_hi{index}', '0')
        return True
    bits = args[0].size * 8
    read = lambda operand, width=bits: _read(function, ins, operand, width)
    if op in ('vmovdqa', 'vmovaps'):
        _aligned(function, ins, bits)
    if op in MOVES:
        _write(function, ins, args[0], read(args[1]), bits)
    elif op in ('vmovd', 'vmovq'):
        width = 32 if op == 'vmovd' else 64
        _write(function, ins, args[0], read(args[1], width), width)
    elif op in BITWISE:
        value = '0' if BITWISE[op] == 'xor' and _same(args[1], args[2]) else function.emit(f'{BITWISE[op]} i{bits} {read(args[1])}, {read(args[2])}')
        _write(function, ins, args[0], value, bits)
    elif op == 'vpbroadcastd':
        count = bits // 32
        scalar = read(args[1], 32)
        lane = function.emit(f'insertelement <{count} x i32> zeroinitializer, i32 {scalar}, i32 0')
        value = function.emit(f'shufflevector <{count} x i32> {lane}, <{count} x i32> zeroinitializer, <{count} x i32> zeroinitializer')
        _write(function, ins, args[0], function.emit(f'bitcast <{count} x i32> {value} to i{bits}'), bits)
    elif op == 'vpshufd':
        count = bits // 32
        source = function.emit(f'bitcast i{bits} {read(args[1])} to <{count} x i32>')
        mask = [(lane // 4) * 4 + ((args[2].imm >> ((lane % 4) * 2)) & 3) for lane in range(count)]
        indices = ', '.join(f'i32 {index}' for index in mask)
        value = function.emit(f'shufflevector <{count} x i32> {source}, <{count} x i32> zeroinitializer, <{count} x i32> <{indices}>')
        _write(function, ins, args[0], function.emit(f'bitcast <{count} x i32> {value} to i{bits}'), bits)
    elif op == 'vpsllvd':
        count = bits // 32
        left, shifts = (function.emit(f'bitcast i{bits} {read(operand)} to <{count} x i32>') for operand in args[1:3])
        threshold = '<' + ', '.join(['i32 32'] * count) + '>'
        mask = '<' + ', '.join(['i32 31'] * count) + '>'
        valid = function.emit(f'icmp ult <{count} x i32> {shifts}, {threshold}')
        safe = function.emit(f'and <{count} x i32> {shifts}, {mask}')
        shifted = function.emit(f'shl <{count} x i32> {left}, {safe}')
        value = function.emit(f'select <{count} x i1> {valid}, <{count} x i32> {shifted}, <{count} x i32> zeroinitializer')
        _write(function, ins, args[0], function.emit(f'bitcast <{count} x i32> {value} to i{bits}'), bits)
    return True
