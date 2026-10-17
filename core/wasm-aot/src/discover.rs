use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};

use iced_x86::{Code, Decoder, DecoderOptions, FlowControl, Instruction, Mnemonic, OpKind, Register};

use crate::image::{Image, SymbolKind};
use crate::program::{Program, R_X86_64_RELATIVE, Target};
use crate::{Error, Result};

pub struct Block {
    pub start: u64,
    pub end: u64,
    pub instructions: Vec<Instruction>,
}

pub struct Function {
    pub image: u32,
    pub start: u64,
    pub end: u64,
    pub blocks: Vec<Block>,
    pub entries: Vec<u64>,
    pub jump_tables: HashMap<u64, Vec<u64>>,
    pub guessed: HashSet<u64>,
}

impl Function {
    pub fn contains(&self, vaddr: u64) -> bool {
        (self.start..self.end).contains(&vaddr)
    }

    pub fn block_index(&self, vaddr: u64) -> Option<usize> {
        self.blocks.binary_search_by_key(&vaddr, |block| block.start).ok()
    }
}

#[derive(Default)]
pub struct Discovery {
    pub functions: Vec<Function>,
    pub invalid: Vec<(u32, u64)>,
    pub unresolved_jumps: Vec<(u32, u64)>,
    pub pointer_jumps: Vec<(u32, u64)>,
    pub guessed_tables: Vec<(u32, u64)>,
}

impl Discovery {
    pub fn function_at(&self, image: u32, vaddr: u64) -> Option<usize> {
        let index = self.functions.partition_point(|function| (function.image, function.start) <= (image, vaddr));
        let candidate = index.checked_sub(1)?;
        let function = &self.functions[candidate];
        (function.image == image && function.contains(vaddr)).then_some(candidate)
    }
}

pub fn discover(program: &Program) -> Result<Discovery> {
    let mut discovery = Discovery::default();
    for (index, image) in program.images.iter().enumerate() {
        discover_image(program, index as u32, image, &mut discovery)?;
    }
    Ok(discovery)
}

struct Roots {
    function_like: BTreeSet<u64>,
    calls: BTreeSet<u64>,
    external: BTreeSet<u64>,
}

fn discover_image(program: &Program, image_number: u32, image: &Image, discovery: &mut Discovery) -> Result<()> {
    let relative: HashMap<u64, u64> = image
        .relocations
        .iter()
        .filter(|relocation| relocation.kind == R_X86_64_RELATIVE)
        .map(|relocation| (relocation.offset, relocation.addend as u64))
        .collect();
    let pointer_at = |vaddr: u64| relative.get(&vaddr).copied().or_else(|| image.read_u64(vaddr));

    let mut roots = Roots { function_like: BTreeSet::new(), calls: BTreeSet::new(), external: BTreeSet::new() };
    let add_root = |roots: &mut Roots, vaddr: u64| {
        if image.is_code(vaddr) {
            roots.function_like.insert(vaddr);
            roots.external.insert(vaddr);
        }
    };
    let add_entry = |roots: &mut Roots, vaddr: u64| {
        if image.is_executable(vaddr) {
            roots.function_like.insert(vaddr);
            roots.external.insert(vaddr);
        }
    };
    for vaddr in [image.entry, image.init, image.fini].into_iter().flatten() {
        add_entry(&mut roots, vaddr);
    }
    for (vaddr, count) in [image.init_array, image.fini_array].into_iter().flatten() {
        for index in 0..count as u64 {
            if let Some(target) = pointer_at(vaddr + 8 * index) {
                add_entry(&mut roots, target);
            }
        }
    }
    for symbol in &image.symbols {
        if let (Some(value), SymbolKind::Function) = (symbol.value, symbol.kind) {
            add_root(&mut roots, value);
        }
    }
    for &target in relative.values() {
        add_root(&mut roots, target);
    }
    for &vaddr in &image.extra_entries {
        add_root(&mut roots, vaddr);
    }
    let fde_ranges: BTreeMap<u64, u64> = image.fdes.iter().map(|fde| (fde.start, fde.end)).collect();
    for fde in &image.fdes {
        add_root(&mut roots, fde.start);
        if let Some(lsda) = fde.lsda {
            for pad in landing_pads(image, fde.start, lsda)? {
                roots.external.insert(pad);
            }
        }
    }

    let mut decoded: BTreeMap<u64, Instruction> = BTreeMap::new();
    let mut jump_tables: HashMap<u64, Vec<u64>> = HashMap::new();
    let mut worklist: Vec<(u64, bool)> = roots.function_like.iter().chain(roots.external.iter()).map(|&vaddr| (vaddr, false)).collect();
    let mut pending: Vec<(Instruction, bool)> = Vec::new();
    let mut guessed: HashSet<u64> = HashSet::new();
    let mut guessing = false;
    loop {
        while let Some((start, soft)) = worklist.pop() {
            if decoded.contains_key(&start) {
                continue;
            }
            let fits = check_no_overlap(&decoded, image, start).is_ok() && image.is_executable(start);
            if !fits {
                if soft {
                    continue;
                }
                check_no_overlap(&decoded, image, start)?;
                return Err(Error::new(format!("{}: control flow reaches {start:#x}, outside the code segments", image.name)));
            }
            let Some(trace) = decode_trace(program, image_number, image, &fde_ranges, &decoded, start, soft)? else { continue };
            for instruction in &trace.instructions {
                decoded.insert(instruction.ip(), *instruction);
                if soft {
                    guessed.insert(instruction.ip());
                }
            }
            if let Some(ip) = trace.invalid {
                discovery.invalid.push((image_number, ip));
            }
            for &target in &trace.code_pointers {
                roots.function_like.insert(target);
                roots.external.insert(target);
            }
            roots.calls.extend(&trace.calls);
            worklist.extend(trace.targets.iter().chain(&trace.code_pointers).chain(&trace.calls).map(|&target| (target, soft)));
            if let Some((ip, targets)) = trace.table {
                worklist.extend(targets.iter().map(|&target| (target, soft)));
                jump_tables.insert(ip, targets);
            }
            pending.extend(trace.pending.into_iter().map(|jump| (jump, soft)));
        }
        let labels: BTreeSet<u64> = if guessing { data_labels(&decoded) } else { BTreeSet::new() };
        let mut progress = false;
        let mut unresolved = Vec::new();
        for (jump, soft) in pending.drain(..) {
            let mut window: Vec<Instruction> = decoded.range(..jump.ip()).rev().take(256).map(|(_, instruction)| *instruction).collect();
            window.reverse();
            let guess = guessing.then_some((&labels, &decoded));
            match jump_table(image, &fde_ranges, &window, &jump, guess) {
                Table::Found(targets, guessed) => {
                    if guessed {
                        discovery.guessed_tables.push((image_number, jump.ip()));
                    }
                    worklist.extend(targets.iter().map(|&target| (target, soft || guessed)));
                    jump_tables.insert(jump.ip(), targets);
                    progress = true;
                }
                Table::Unrecognized => unresolved.push((jump, soft, false)),
                Table::Unresolved => unresolved.push((jump, soft, true)),
            }
        }
        if !progress && guessing {
            for (jump, _, table_shaped) in unresolved {
                if table_shaped {
                    discovery.unresolved_jumps.push((image_number, jump.ip()));
                } else {
                    discovery.pointer_jumps.push((image_number, jump.ip()));
                }
            }
            break;
        }
        if !progress {
            guessing = true;
        }
        pending = unresolved.into_iter().map(|(jump, soft, _)| (jump, soft)).collect();
    }

    let mut starts: BTreeSet<u64> = fde_ranges.keys().copied().collect();
    let inside_fde = |vaddr: u64| fde_ranges.range(..=vaddr).next_back().is_some_and(|(&start, &end)| vaddr != start && vaddr < end);
    for &vaddr in roots.function_like.iter().chain(roots.calls.iter()) {
        if !inside_fde(vaddr) {
            starts.insert(vaddr);
        }
    }
    let mut ranges: BTreeMap<u64, u64> = BTreeMap::new();
    loop {
        ranges.clear();
        for &start in &starts {
            let end = match fde_ranges.get(&start) {
                Some(&end) => end,
                None => {
                    let next = starts.range(start + 1..).next().copied().unwrap_or(u64::MAX);
                    let segment_end = image.segment_at(start).map_or(start, |segment| segment.end());
                    next.min(segment_end)
                }
            };
            ranges.insert(start, end);
        }
        let orphan = decoded.keys().find(|&&ip| ranges.range(..=ip).next_back().is_none_or(|(_, &end)| ip >= end));
        match orphan {
            Some(&ip) => {
                starts.insert(ip);
            }
            None => break,
        }
    }
    let function_of = |vaddr: u64| ranges.range(..=vaddr).next_back().filter(|(_, end)| vaddr < **end).map(|(start, _)| *start);

    let mut entries: BTreeMap<u64, BTreeSet<u64>> = ranges.keys().map(|&start| (start, BTreeSet::from([start]))).collect();
    let mut leaders: BTreeMap<u64, BTreeSet<u64>> = ranges.keys().map(|&start| (start, BTreeSet::from([start]))).collect();
    let mark_entry = |entries: &mut BTreeMap<u64, BTreeSet<u64>>, leaders: &mut BTreeMap<u64, BTreeSet<u64>>, vaddr: u64| {
        if let Some(function) = function_of(vaddr) {
            entries.get_mut(&function).unwrap().insert(vaddr);
            leaders.get_mut(&function).unwrap().insert(vaddr);
        }
    };
    for &vaddr in &roots.external {
        if decoded.contains_key(&vaddr) {
            mark_entry(&mut entries, &mut leaders, vaddr);
        }
    }
    for (&ip, instruction) in &decoded {
        let own = function_of(ip).unwrap();
        let mut successors: Vec<u64> = Vec::new();
        match instruction.flow_control() {
            FlowControl::ConditionalBranch | FlowControl::UnconditionalBranch => successors.push(instruction.near_branch_target()),
            FlowControl::IndirectBranch => successors.extend(jump_tables.get(&ip).into_iter().flatten()),
            _ => {}
        }
        let ends_block = !matches!(instruction.flow_control(), FlowControl::Next) || instruction.mnemonic() == Mnemonic::Hlt;
        if matches!(instruction.flow_control(), FlowControl::Call | FlowControl::IndirectCall) {
            mark_entry(&mut entries, &mut leaders, instruction.next_ip());
        }
        if instruction.flow_control() == FlowControl::Call {
            mark_entry(&mut entries, &mut leaders, instruction.near_branch_target());
        }
        if let Some(Target::Guest { image: target_image, vaddr }) = slot_operand(program, image_number, instruction)
            && target_image == image_number
        {
            mark_entry(&mut entries, &mut leaders, vaddr);
        }
        if matches!(instruction.flow_control(), FlowControl::Next | FlowControl::ConditionalBranch) && instruction.code() != Code::INVALID {
            successors.push(instruction.next_ip());
        }
        for target in successors {
            match function_of(target) {
                Some(function) if function == own => {
                    leaders.get_mut(&own).unwrap().insert(target);
                }
                Some(_) => mark_entry(&mut entries, &mut leaders, target),
                None => {
                    return Err(Error::new(format!("{}: branch at {ip:#x} leaves the code to {target:#x}", image.name)));
                }
            }
        }
        if ends_block
            && let Some(next) = decoded.range(instruction.next_ip()..).next().map(|(&next, _)| next)
            && function_of(next) == Some(own)
        {
            leaders.get_mut(&own).unwrap().insert(next);
        }
    }

    for (&start, &end) in &ranges {
        let function_leaders = &leaders[&start];
        let mut blocks: Vec<Block> = Vec::new();
        let mut force_new = true;
        for (&ip, instruction) in decoded.range(start..end) {
            let continues = !force_new && blocks.last().is_some_and(|block| block.end == ip) && !function_leaders.contains(&ip);
            if !continues {
                blocks.push(Block { start: ip, end: ip, instructions: Vec::new() });
            }
            let block = blocks.last_mut().unwrap();
            block.instructions.push(*instruction);
            block.end = instruction.next_ip();
            force_new = !matches!(instruction.flow_control(), FlowControl::Next) || instruction.mnemonic() == Mnemonic::Hlt;
        }
        if blocks.is_empty() {
            continue;
        }
        let tables = jump_tables.iter().filter(|(ip, _)| (start..end).contains(*ip)).map(|(&ip, targets)| (ip, targets.clone())).collect();
        let mut function_entries: Vec<u64> = entries[&start].iter().copied().filter(|vaddr| decoded.contains_key(vaddr)).collect();
        function_entries.sort_by_key(|&vaddr| (vaddr != start, vaddr));
        let function_guessed = guessed.iter().copied().filter(|ip| (start..end).contains(ip)).collect();
        discovery.functions.push(Function {
            image: image_number,
            start,
            end,
            blocks,
            entries: function_entries,
            jump_tables: tables,
            guessed: function_guessed,
        });
    }
    Ok(())
}

fn check_no_overlap(decoded: &BTreeMap<u64, Instruction>, image: &Image, start: u64) -> Result<()> {
    if let Some((&previous, instruction)) = decoded.range(..start).next_back()
        && instruction.next_ip() > start
    {
        return Err(Error::new(format!("{}: {start:#x} is inside the instruction at {previous:#x}", image.name)));
    }
    Ok(())
}

fn code_pointer(image: &Image, instruction: &Instruction) -> Option<u64> {
    if instruction.mnemonic() == Mnemonic::Lea && instruction.is_ip_rel_memory_operand() {
        let target = instruction.ip_rel_memory_address();
        return image.is_code(target).then_some(target);
    }
    None
}

pub fn slot_operand(program: &Program, image: u32, instruction: &Instruction) -> Option<Target> {
    if instruction.op0_kind() == OpKind::Memory && instruction.is_ip_rel_memory_operand() {
        return program.slot_target(image, instruction.ip_rel_memory_address());
    }
    None
}

struct Trace {
    instructions: Vec<Instruction>,
    invalid: Option<u64>,
    targets: Vec<u64>,
    calls: Vec<u64>,
    code_pointers: Vec<u64>,
    table: Option<(u64, Vec<u64>)>,
    pending: Vec<Instruction>,
}

fn decode_trace(
    program: &Program,
    image_number: u32,
    image: &Image,
    fde_ranges: &BTreeMap<u64, u64>,
    decoded: &BTreeMap<u64, Instruction>,
    start: u64,
    soft: bool,
) -> Result<Option<Trace>> {
    let mut trace =
        Trace { instructions: Vec::new(), invalid: None, targets: Vec::new(), calls: Vec::new(), code_pointers: Vec::new(), table: None, pending: Vec::new() };
    let bytes = image.bytes_at(start).unwrap_or(&[]);
    let mut decoder = Decoder::with_ip(64, bytes, start, DecoderOptions::NONE);
    while decoder.can_decode() {
        let instruction = decoder.decode();
        let ip = instruction.ip();
        if decoded.contains_key(&ip) {
            break;
        }
        if let Some((&next, _)) = decoded.range(ip + 1..).next()
            && next < instruction.next_ip()
        {
            if soft {
                return Ok(None);
            }
            return Err(Error::new(format!("{}: instructions overlap at {ip:#x} and {next:#x}", image.name)));
        }
        if instruction.code() == Code::INVALID {
            if soft {
                return Ok(None);
            }
            trace.instructions.push(instruction);
            trace.invalid = Some(ip);
            break;
        }
        trace.instructions.push(instruction);
        if let Some(target) = code_pointer(image, &instruction) {
            trace.code_pointers.push(target);
        }
        match instruction.flow_control() {
            FlowControl::Next => {
                if instruction.mnemonic() == Mnemonic::Hlt {
                    break;
                }
            }
            FlowControl::ConditionalBranch => trace.targets.push(instruction.near_branch_target()),
            FlowControl::UnconditionalBranch => {
                trace.targets.push(instruction.near_branch_target());
                break;
            }
            FlowControl::Call => trace.calls.push(instruction.near_branch_target()),
            FlowControl::IndirectCall => {
                if let Some(Target::Guest { image: target_image, vaddr }) = slot_operand(program, image_number, &instruction)
                    && target_image == image_number
                {
                    trace.calls.push(vaddr);
                }
            }
            FlowControl::IndirectBranch => {
                if slot_operand(program, image_number, &instruction).is_none() {
                    let recent = &trace.instructions[trace.instructions.len().saturating_sub(25)..trace.instructions.len() - 1];
                    match jump_table(image, fde_ranges, recent, &instruction, None) {
                        Table::Found(targets, _) => trace.table = Some((ip, targets)),
                        _ => trace.pending.push(instruction),
                    }
                }
                break;
            }
            FlowControl::Return | FlowControl::Interrupt | FlowControl::Exception | FlowControl::XbeginXabortXend => break,
        }
    }
    Ok(Some(trace))
}

enum Table {
    Found(Vec<u64>, bool),
    Unresolved,
    Unrecognized,
}

fn find_before(recent: &[Instruction], before: usize, predicate: impl Fn(&Instruction) -> bool) -> Option<usize> {
    (0..before).rev().find(|&position| predicate(&recent[position]))
}

fn jump_table(
    image: &Image,
    fde_ranges: &BTreeMap<u64, u64>,
    recent: &[Instruction],
    jump: &Instruction,
    guess: Option<(&BTreeSet<u64>, &BTreeMap<u64, Instruction>)>,
) -> Table {
    if jump.op0_kind() != OpKind::Register {
        return Table::Unrecognized;
    }
    let jump_register = jump.op0_register().full_register();
    let Some(writer) = find_before(recent, recent.len(), |instruction| {
        instruction.op_count() > 0 && instruction.op0_kind() == OpKind::Register && instruction.op0_register().full_register() == jump_register
    }) else {
        return Table::Unrecognized;
    };
    let add = writer;
    let is_add = recent[add].mnemonic() == Mnemonic::Add && recent[add].op1_kind() == OpKind::Register;
    if !is_add {
        return Table::Unrecognized;
    }
    let other = recent[add].op1_register().full_register();
    let mut shape = None;
    for (offset_register, base_register) in [(jump_register, other), (other, jump_register)] {
        let load = find_before(recent, add, |instruction| {
            instruction.mnemonic() == Mnemonic::Movsxd
                && instruction.op0_register().full_register() == offset_register
                && instruction.op1_kind() == OpKind::Memory
                && instruction.memory_base().full_register() == base_register
                && instruction.memory_index_scale() == 4
                && instruction.memory_displacement64() == 0
        });
        if let Some(load) = load {
            shape = Some((load, base_register));
            break;
        }
    }
    let Some((load, base_register)) = shape else { return Table::Unrecognized };
    let index_register = recent[load].memory_index().full_register();
    let lea = find_before(recent, load, |instruction| {
        instruction.mnemonic() == Mnemonic::Lea && instruction.op0_register().full_register() == base_register && instruction.is_ip_rel_memory_operand()
    });
    let Some(lea) = lea else { return Table::Unresolved };
    let table = recent[lea].ip_rel_memory_address();
    let bound = jump_table_bound(&recent[..load], index_register);
    let function = fde_ranges.range(..=jump.ip()).next_back().filter(|(_, end)| jump.ip() < **end).map(|(&start, &end)| start..end);
    let valid = |target: u64| image.is_code(target) && function.as_ref().is_none_or(|range| range.contains(&target));
    if let Some(count) = bound {
        let targets: Option<Vec<u64>> = (0..count)
            .map(|entry| image.read_u32(table + 4 * entry).map(|offset| table.wrapping_add(offset as i32 as i64 as u64)).filter(|&target| valid(target)))
            .collect();
        match targets {
            Some(targets) if !targets.is_empty() => return Table::Found(targets, false),
            _ => {}
        }
    }
    let Some((labels, decoded)) = guess else { return Table::Unresolved };
    let mut targets = Vec::new();
    for entry in 0..MAX_GUESSED_ENTRIES {
        let address = table + 4 * entry;
        if entry > 0 && labels.contains(&address) {
            break;
        }
        let Some(offset) = image.read_u32(address) else { break };
        let target = table.wrapping_add(offset as i32 as i64 as u64);
        let inside_instruction = decoded.range(..target).next_back().is_some_and(|(_, instruction)| instruction.next_ip() > target);
        if !valid(target) || inside_instruction {
            break;
        }
        targets.push(target);
    }
    if targets.is_empty() {
        return Table::Unresolved;
    }
    Table::Found(targets, true)
}

fn data_labels(decoded: &BTreeMap<u64, Instruction>) -> BTreeSet<u64> {
    decoded.values().filter(|instruction| instruction.is_ip_rel_memory_operand()).map(Instruction::ip_rel_memory_address).collect()
}

const MAX_GUESSED_ENTRIES: u64 = 256;

fn jump_table_bound(recent: &[Instruction], index_register: Register) -> Option<u64> {
    let barrier = recent.iter().rposition(|instruction| {
        matches!(instruction.flow_control(), FlowControl::Return | FlowControl::UnconditionalBranch | FlowControl::IndirectBranch | FlowControl::Interrupt)
    });
    let recent = &recent[barrier.map_or(0, |position| position + 1)..];
    let mut aliases = vec![index_register];
    let mut changed = true;
    while changed {
        changed = false;
        for instruction in recent {
            let copies = matches!(instruction.mnemonic(), Mnemonic::Mov | Mnemonic::Movzx | Mnemonic::Movsx | Mnemonic::Movsxd);
            if !copies || instruction.op0_kind() != OpKind::Register || instruction.op1_kind() != OpKind::Register {
                continue;
            }
            let (destination, source) = (instruction.op0_register().full_register(), instruction.op1_register().full_register());
            for (known, other) in [(destination, source), (source, destination)] {
                if aliases.contains(&known) && !aliases.contains(&other) {
                    aliases.push(other);
                    changed = true;
                }
            }
        }
    }
    for (position, instruction) in recent.iter().enumerate().rev() {
        let compares_index = instruction.mnemonic() == Mnemonic::Cmp
            && instruction.op0_kind() == OpKind::Register
            && aliases.contains(&instruction.op0_register().full_register())
            && matches!(
                instruction.op1_kind(),
                OpKind::Immediate8
                    | OpKind::Immediate8to16
                    | OpKind::Immediate16
                    | OpKind::Immediate8to32
                    | OpKind::Immediate8to64
                    | OpKind::Immediate32
                    | OpKind::Immediate32to64
            );
        if !compares_index {
            continue;
        }
        let limit = instruction.immediate(1);
        let count = match recent.get(position + 1)?.mnemonic() {
            Mnemonic::Ja => limit + 1,
            Mnemonic::Jae => limit,
            _ => return None,
        };
        return (count <= MAX_TABLE_ENTRIES).then_some(count);
    }
    None
}

const MAX_TABLE_ENTRIES: u64 = 4096;

fn landing_pads(image: &Image, function_start: u64, lsda: u64) -> Result<Vec<u64>> {
    let bytes = image.bytes_at(lsda).ok_or_else(|| Error::new(format!("{}: LSDA at {lsda:#x} is outside the segments", image.name)))?;
    let table = crate::lsda::parse(bytes, lsda, function_start).ok_or_else(|| Error::new(format!("{}: LSDA at {lsda:#x} is malformed", image.name)))?;
    Ok(table.call_sites.iter().filter(|site| site.landing_pad != 0).map(|site| site.landing_pad).collect())
}
