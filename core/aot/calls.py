from capstone.x86 import X86_OP_MEM, X86_OP_REG

import abi


class CallError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise CallError(message)


def structural_signature(contract):
    try:
        result, parameters = abi.signature(contract)
    except abi.ABIError as error:
        raise CallError(str(error)) from error
    return (abi.result_type(result), tuple(abi.parameter_type(kind) for kind in parameters),
            abi.return_register(result),
            tuple((item['kind'], item.get('name'), item.get('offset'), item['bits']) for item in abi.locations(parameters)))


def validate_operand(ins):
    require(ins.mnemonic == 'call' and len(ins.operands) == 1, 'indirect annotation does not identify a call')
    operand = ins.operands[0]
    require(operand.type in (X86_OP_REG, X86_OP_MEM) and operand.size == 8,
            'indirect annotation requires a 64-bit register or memory call')
    require(ins.addr_size == 8, 'indirect call requires 64-bit addressing')
    if operand.type == X86_OP_MEM:
        require(not operand.mem.segment, 'segment-relative indirect calls require TLS reconstruction')


def validate_indirect_calls(image, owner_contract):
    records = owner_contract.get('indirect_calls', [])
    require(isinstance(records, list), 'indirect_calls must be a list')
    start, size = owner_contract['address'], owner_contract['size']
    require(type(start) is int and type(size) is int and start >= 0 and size > 0,
            'invalid indirect call owner range')
    descriptors = {}
    for record in records:
        require(isinstance(record, dict), 'indirect call annotation must be an object')
        address = record.get('address')
        require(type(address) is int and start <= address < start + size,
                'indirect call address is outside owner function')
        require(address not in descriptors, 'duplicate indirect call annotation')
        decoded = list(image.decoder.disasm(image.read(address, min(15, start + size - address)), address, count=1))
        require(len(decoded) == 1 and decoded[0].address + decoded[0].size <= start + size,
                'truncated indirect call instruction')
        validate_operand(decoded[0])
        candidate = {'name': f'indirect_{address:x}', 'result': record.get('result'),
                     'parameters': record.get('parameters')}
        for field in ('variadic', 'sret', 'byval', 'guest_calling_convention', 'guest_locations', 'guest_return_register'):
            if field in record:
                candidate[field] = record[field]
        expected = structural_signature(candidate)
        candidate['parameters'] = list(candidate['parameters'])
        targets = record.get('targets')
        require(isinstance(targets, list) and targets, 'indirect call requires a nonempty declared target set')
        require(all(type(target) is int and 0 <= target <= (1 << 64) - 1 for target in targets),
                'invalid indirect call target address')
        require(len(set(targets)) == len(targets), 'duplicate indirect call target address')
        names, resolved = [], []
        for target in targets:
            try:
                contract = image.functions[target] if target in image.functions else image.callee(target)
            except ValueError as error:
                raise CallError(f'unresolved indirect call target 0x{target:x}: {error}') from error
            require(structural_signature(contract) == expected,
                    f'indirect call ABI does not match target {contract["name"]}')
            if contract['name'] not in names:
                names.append(contract['name'])
            resolved.append(contract)
        all_noreturn = all(bool(contract.get('noreturn')) for contract in resolved)
        if 'noreturn' in record:
            require(type(record['noreturn']) is bool and (not record['noreturn'] or all_noreturn),
                    'indirect call noreturn annotation contradicts a target')
        if all_noreturn:
            candidate['noreturn'] = True
        descriptors[address] = {'contract': candidate, 'kind': 'operand', 'targets': names,
                                'target_addresses': list(targets),
                                'qualification': 'declared-target-set-not-inferred',
                                'source_qualification': 'Caller-supplied closed-world target set; no automatic target proof or runtime membership check',
                                'native_pointer_requirement': 'operand must already contain a native pointer to a declared target'}
    return descriptors


def indirect_call(image, ins, owner_contract):
    validate_operand(ins)
    descriptors = validate_indirect_calls(image, owner_contract)
    require(ins.address in descriptors, 'indirect call has no signature and target-set annotation')
    expected = image.read(ins.address, ins.size)
    require(bytes(ins.bytes) == expected, 'indirect call does not match input instruction bytes')
    return descriptors[ins.address]
