use wasm_encoder::{Function, InstructionSink, MemArg, ValType};

const KINDS: usize = 5;

fn kind(ty: ValType) -> usize {
    match ty {
        ValType::I32 => 0,
        ValType::I64 => 1,
        ValType::F32 => 2,
        ValType::F64 => 3,
        ValType::V128 => 4,
        ValType::Ref(_) => panic!("reference temporaries are not used"),
    }
}

pub struct Body {
    bytes: Vec<u8>,
    locals: Vec<ValType>,
    parameters: u32,
    pools: [Vec<u32>; KINDS],
    taken: [usize; KINDS],
}

impl Body {
    pub fn new(parameters: u32, fixed: &[ValType]) -> Self {
        Self { bytes: Vec::new(), locals: fixed.to_vec(), parameters, pools: Default::default(), taken: [0; KINDS] }
    }

    pub fn sink(&mut self) -> InstructionSink<'_> {
        InstructionSink::new(&mut self.bytes)
    }

    pub fn temp(&mut self, ty: ValType) -> u32 {
        let index = kind(ty);
        if self.taken[index] == self.pools[index].len() {
            self.pools[index].push(self.parameters + self.locals.len() as u32);
            self.locals.push(ty);
        }
        let local = self.pools[index][self.taken[index]];
        self.taken[index] += 1;
        local
    }

    pub fn release_temps(&mut self) {
        self.taken = [0; KINDS];
    }

    pub fn finish(self) -> Function {
        let mut function = Function::new_with_locals_types(self.locals);
        function.raw(self.bytes);
        function
    }
}

pub fn mem(offset: u64) -> MemArg {
    MemArg { offset, align: 0, memory_index: 0 }
}

pub fn aligned(offset: u64, size_log2: u32) -> MemArg {
    MemArg { offset, align: size_log2, memory_index: 0 }
}
