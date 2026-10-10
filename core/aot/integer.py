from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG


FLAGS = {'cf', 'zf', 'sf', 'of', 'pf'}
CONDITIONS = {
    'e': {'zf'}, 'ne': {'zf'}, 'a': {'cf', 'zf'}, 'ae': {'cf'}, 'b': {'cf'}, 'be': {'cf', 'zf'},
    'g': {'zf', 'sf', 'of'}, 'ge': {'sf', 'of'}, 'l': {'sf', 'of'}, 'le': {'zf', 'sf', 'of'},
    's': {'sf'}, 'ns': {'sf'}, 'o': {'of'}, 'no': {'of'}, 'p': {'pf'}, 'np': {'pf'},
}
ALIASES = {'z': 'e', 'nz': 'ne', 'c': 'b', 'nc': 'ae', 'na': 'be', 'nbe': 'a',
           'nb': 'ae', 'nae': 'b', 'ng': 'le', 'nge': 'l', 'nl': 'ge', 'nle': 'g', 'pe': 'p', 'po': 'np'}
SHIFTS = {'shl', 'sal', 'shr', 'sar'}
OPERATIONS = {'inc', 'dec', 'imul', 'sbb', 'adc', 'cdqe', 'cdq', 'cwd', 'cqo', 'neg', 'not', 'div', 'blsr', 'bt'} | SHIFTS


def condition(ins):
    suffix = ins.mnemonic[4:] if ins.mnemonic.startswith('cmov') else ins.mnemonic[3:]
    return ALIASES.get(suffix, suffix)


def implicit_define(state, name, width, shift=0):
    mask = ((1 << width) - 1) << shift
    state['known'][name] = (1 << 64) - 1 if width >= 32 else state['known'].get(name, 0) | mask
    state['pointers'].pop(name, None)


def analyze(function, ins, state):
    op, args = ins.mnemonic, ins.operands
    conditional = op.startswith(('cmov', 'set'))
    if op not in OPERATIONS and not conditional:
        return False
    if conditional:
        if args and args[0].type == X86_OP_REG and function.reg(ins, args[0])[0] in ('rsp', 'rbp'):
            function.fail(ins, 'conditional operation cannot reconstruct a dynamic native frame')
        suffix = condition(ins)
        if suffix not in CONDITIONS:
            function.fail(ins, 'unsupported integer condition')
        if not CONDITIONS[suffix] <= state['flags']:
            function.fail(ins, 'undefined integer condition flags')
        if op.startswith('cmov'):
            if len(args) != 2 or args[0].type != X86_OP_REG or args[0].size not in (2, 4, 8):
                function.fail(ins, 'unsupported conditional move operands')
            for operand in args:
                function.read_operand(ins, state, operand)
        elif len(args) != 1 or args[0].size != 1:
            function.fail(ins, 'unsupported conditional set operands')
        function.define(ins, state, args[0])
        return True
    if op in ('blsr', 'bt'):
        if len(args) != 2 or args[0].type != X86_OP_REG:
            function.fail(ins, 'bit operation requires a register destination')
        if op == 'bt':
            if args[0].size not in (2, 4, 8) or args[1].type not in (X86_OP_REG, X86_OP_IMM):
                function.fail(ins, 'unsupported register bit test')
            for operand in args:
                function.read_operand(ins, state, operand)
            state['flags'] = {'cf'} | (state['flags'] & {'zf'})
        else:
            if args[0].size not in (4, 8) or args[1].size != args[0].size:
                function.fail(ins, 'unsupported reset-lowest-bit width')
            if function.reg(ins, args[0])[0] in ('rsp', 'rbp'):
                function.fail(ins, 'bit operation cannot reconstruct a dynamic native frame')
            function.read_operand(ins, state, args[1])
            function.define(ins, state, args[0])
            state['flags'] = {'cf', 'zf', 'sf', 'of'}
        return True
    if op in ('cdqe', 'cdq', 'cwd', 'cqo'):
        width = {'cdqe': 32, 'cdq': 32, 'cwd': 16, 'cqo': 64}[op]
        function.check_register(ins, state, 'rax', width)
        implicit_define(state, 'rax' if op == 'cdqe' else 'rdx', 64 if op == 'cdqe' else width)
        return True
    if op == 'div':
        if function.contract.get('integer_division') != 'nonzero-no-overflow':
            function.fail(ins, 'integer division requires a verified nonzero-no-overflow contract')
        width = args[0].size * 8
        function.read_operand(ins, state, args[0])
        function.check_register(ins, state, 'rax', 16 if width == 8 else width)
        if width != 8:
            function.check_register(ins, state, 'rdx', width)
        implicit_define(state, 'rax', 16 if width == 8 else width)
        if width != 8:
            implicit_define(state, 'rdx', width)
        state['flags'] = set()
        return True
    if args[0].type == X86_OP_REG and function.reg(ins, args[0])[0] in ('rsp', 'rbp'):
        function.fail(ins, 'integer operation cannot reconstruct a dynamic native frame')
    if op == 'imul' and len(args) not in (2, 3):
        function.fail(ins, 'one-operand signed multiplication is not implemented')
    reads = args[1:] if op == 'imul' and len(args) == 3 else args
    for operand in reads:
        function.read_operand(ins, state, operand)
    if op in ('adc', 'sbb') and 'cf' not in state['flags']:
        function.fail(ins, 'carry input is not defined')
    if op in SHIFTS:
        width = args[0].size * 8
        if len(args) != 2 or width not in (8, 16, 32, 64):
            function.fail(ins, 'unsupported shift operands')
        if args[1].type == X86_OP_IMM:
            count = args[1].imm & (63 if width == 64 else 31)
            if count == 0:
                if width == 32:
                    function.define(ins, state, args[0])
                return True
            state['flags'] = {'zf', 'sf', 'pf'}
            if count < width or op == 'sar':
                state['flags'].add('cf')
            if count == 1:
                state['flags'].add('of')
        else:
            available = {'zf', 'sf', 'pf'}
            if width >= 32 or op == 'sar':
                available.add('cf')
            state['flags'] = state['flags'] & available
    elif op in ('inc', 'dec'):
        state['flags'] = {'zf', 'sf', 'of', 'pf'} | (state['flags'] & {'cf'})
    elif op == 'imul':
        state['flags'] = {'cf', 'of'}
    elif op != 'not':
        state['flags'] = FLAGS.copy()
    function.define(ins, state, args[0])
    return True


def load_flag(function, name):
    return function.emit(f'load i1, ptr %{name}, align 1')


def store_flag(function, name, value):
    function.lines.append(f'  store i1 {value}, ptr %{name}, align 1')


def extension(function, value, source_width, target_width, signed=False):
    if source_width == target_width:
        return value
    return function.emit(f'{"sext" if signed else "zext"} i{source_width} {value} to i{target_width}')


def lower_shift(function, ins):
    args, op = ins.operands, ins.mnemonic
    width = args[0].size * 8
    mask = 63 if width == 64 else 31
    if args[1].type == X86_OP_IMM and args[1].imm & mask == 0:
        if width == 32:
            function.write(ins, args[0], function.read(ins, args[0]))
        return
    previous = function.states[ins.address]['flags']
    old = {name: load_flag(function, name) if name in previous else 'false' for name in FLAGS}
    value = function.read(ins, args[0])
    raw_count = function.read(ins, args[1])
    count_width = args[1].size * 8
    raw_count = function.emit(f'and i{count_width} {raw_count}, {mask}')
    wide = 128 if width == 64 else 64
    count = extension(function, raw_count, count_width, wide)
    zero = function.emit(f'icmp eq i{wide} {count}, 0')
    extended = extension(function, value, width, wide, op == 'sar')
    instruction = {'shl': 'shl', 'sal': 'shl', 'shr': 'lshr', 'sar': 'ashr'}[op]
    shifted = function.emit(f'{instruction} i{wide} {extended}, {count}')
    result = function.emit(f'trunc i{wide} {shifted} to i{width}')
    function.flags('and', width, value, '0', result)
    high = function.emit(f'icmp ult i{wide} {count}, {width + 1}')
    if op in ('shl', 'sal'):
        position = function.emit(f'sub i{wide} {width}, {count}')
        position = function.emit(f'select i1 {high}, i{wide} {position}, i{wide} 0')
        carry_value = function.emit(f'lshr i{wide} {extended}, {position}')
    else:
        position = function.emit(f'sub i{wide} {count}, 1')
        position = function.emit(f'select i1 {zero}, i{wide} 0, i{wide} {position}')
        carry_value = function.emit(f'{instruction} i{wide} {extended}, {position}')
    carry = function.emit(f'trunc i{wide} {carry_value} to i1')
    if op in ('shl', 'sal'):
        sign = function.emit(f'icmp slt i{width} {result}, 0')
        overflow = function.emit(f'xor i1 {sign}, {carry}')
    elif op == 'shr':
        overflow = function.emit(f'icmp slt i{width} {value}, 0')
    else:
        overflow = 'false'
    store_flag(function, 'cf', carry)
    store_flag(function, 'of', overflow)
    for name in FLAGS:
        current = load_flag(function, name)
        store_flag(function, name, function.emit(f'select i1 {zero}, i1 {old[name]}, i1 {current}'))
    function.write(ins, args[0], result)


def lower_divide(function, ins):
    operand = ins.operands[0]
    width = operand.size * 8
    denominator = function.read(ins, operand)
    low = function.load('rax', 16 if width == 8 else width)
    if width == 8:
        numerator = low
        high = function.emit(f'lshr i16 {low}, 8')
        high = function.emit(f'trunc i16 {high} to i8')
    else:
        high = function.load('rdx', width)
        upper = extension(function, high, width, width * 2)
        upper = function.emit(f'shl i{width * 2} {upper}, {width}')
        low = extension(function, low, width, width * 2)
        numerator = function.emit(f'or i{width * 2} {upper}, {low}')
    valid = function.emit(f'icmp ult i{width} {high}, {denominator}')
    label = f'integer_div_{ins.address:x}_{function.serial}'
    function.lines.extend([f'  br i1 {valid}, label %{label}_ok, label %{label}_fault',
                           f'{label}_fault:', '  call void @llvm.trap()', '  unreachable', f'{label}_ok:'])
    denominator = extension(function, denominator, width, width * 2)
    quotient = function.emit(f'udiv i{width * 2} {numerator}, {denominator}')
    remainder = function.emit(f'urem i{width * 2} {numerator}, {denominator}')
    quotient = function.emit(f'trunc i{width * 2} {quotient} to i{width}')
    remainder = function.emit(f'trunc i{width * 2} {remainder} to i{width}')
    function.store('rax', quotient, width)
    function.store('rax' if width == 8 else 'rdx', remainder, width, 8 if width == 8 else 0)


def lower(function, ins):
    op, args = ins.mnemonic, ins.operands
    conditional = op.startswith(('cmov', 'set'))
    if op not in OPERATIONS and not conditional:
        return False
    if conditional:
        predicate = function.condition('j' + condition(ins))
        if op.startswith('set'):
            value = function.emit(f'zext i1 {predicate} to i8')
        else:
            width = args[0].size * 8
            old, new = function.read(ins, args[0]), function.read(ins, args[1])
            value = function.emit(f'select i1 {predicate}, i{width} {new}, i{width} {old}')
        function.write(ins, args[0], value)
    elif op == 'blsr':
        width = args[0].size * 8
        source = function.read(ins, args[1])
        previous = function.emit(f'sub i{width} {source}, 1')
        result = function.emit(f'and i{width} {previous}, {source}')
        function.flags('and', width, source, '0', result)
        store_flag(function, 'cf', function.emit(f'icmp eq i{width} {source}, 0'))
        function.write(ins, args[0], result)
    elif op == 'bt':
        width = args[0].size * 8
        source = function.read(ins, args[0])
        index = function.read(ins, args[1])
        index_width = args[1].size * 8
        index = function.emit(f'and i{index_width} {index}, {width - 1}')
        index = extension(function, index, index_width, width)
        shifted = function.emit(f'lshr i{width} {source}, {index}')
        store_flag(function, 'cf', function.emit(f'trunc i{width} {shifted} to i1'))
    elif op in ('cdqe', 'cdq', 'cwd', 'cqo'):
        width = {'cdqe': 32, 'cdq': 32, 'cwd': 16, 'cqo': 64}[op]
        value = function.load('rax', width)
        if op == 'cdqe':
            function.store('rax', extension(function, value, 32, 64, True))
        else:
            function.store('rdx', function.emit(f'ashr i{width} {value}, {width - 1}'), width)
    elif op == 'div':
        lower_divide(function, ins)
    elif op in SHIFTS:
        lower_shift(function, ins)
    elif op == 'imul':
        width = args[0].size * 8
        left, right = args if len(args) == 2 else args[1:]
        left = extension(function, function.read(ins, left), width, width * 2, True)
        right = extension(function, function.read(ins, right, width), width, width * 2, True)
        product = function.emit(f'mul i{width * 2} {left}, {right}')
        result = function.emit(f'trunc i{width * 2} {product} to i{width}')
        check = extension(function, result, width, width * 2, True)
        overflow = function.emit(f'icmp ne i{width * 2} {product}, {check}')
        store_flag(function, 'cf', overflow)
        store_flag(function, 'of', overflow)
        function.write(ins, args[0], result)
    else:
        width = args[0].size * 8
        left = function.read(ins, args[0])
        if op == 'not':
            value = function.emit(f'xor i{width} {left}, -1')
        elif op == 'neg':
            value = function.emit(f'sub i{width} 0, {left}')
            function.flags('sub', width, '0', left, value)
        elif op in ('inc', 'dec'):
            old = load_flag(function, 'cf') if 'cf' in function.states[ins.address]['flags'] else 'false'
            instruction = 'add' if op == 'inc' else 'sub'
            value = function.emit(f'{instruction} i{width} {left}, 1')
            function.flags(instruction, width, left, '1', value)
            store_flag(function, 'cf', old)
        else:
            right = function.read(ins, args[1], width)
            carry = load_flag(function, 'cf')
            wide = width + 1
            lhs = extension(function, left, width, wide)
            rhs = extension(function, right, width, wide)
            rhs = function.emit(f'add i{wide} {rhs}, {extension(function, carry, 1, wide)}')
            instruction = 'add' if op == 'adc' else 'sub'
            result = function.emit(f'{instruction} i{wide} {lhs}, {rhs}')
            value = function.emit(f'trunc i{wide} {result} to i{width}')
            if op == 'adc':
                new_carry = function.emit(f'lshr i{wide} {result}, {width}')
                new_carry = function.emit(f'trunc i{wide} {new_carry} to i1')
            else:
                new_carry = function.emit(f'icmp ult i{wide} {lhs}, {rhs}')
            function.flags(instruction, width, left, right, value)
            store_flag(function, 'cf', new_carry)
        function.write(ins, args[0], value)
    return True
