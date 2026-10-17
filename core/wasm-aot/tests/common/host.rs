use std::collections::HashMap;
use std::sync::Arc;

use anyps5_wasm_aot::abi;
use anyps5_wasm_aot::meta::{self, Metadata, RelocKind};
use wasmtime::{Caller, Config, Engine, Global, GlobalType, Linker, Memory, MemoryType, Module, Mutability, Store, TypedFunc, Val, ValType};

pub const MEMORY_SIZE: u64 = 0x4_0010_0000;
pub const CTX_ADDR: u64 = 0x0010_0000;
pub const SAVE_AREA: u64 = 0x0010_8000;
pub const STUB_BASE: u64 = 0x0011_0000;
pub const ADDR_MAP: u64 = 0x0020_0000;
pub const IMAGE_REGION: u64 = 0x0100_0000;
pub const IMAGE_STRIDE: u64 = 0x0080_0000;
pub const TLS_AREA: u64 = 0x0300_0000;
pub const HOST_DATA: u64 = 0x0301_0000;
pub const HEAP_BASE: u64 = 0x0320_0000;
pub const GUEST_STACK_TOP: u64 = 0x03f0_0000;
pub const GUEST_CALL_SENTINEL: u64 = 0xF100;

const ARGUMENT_REGISTERS: [u32; 6] = [7, 6, 2, 1, 8, 9];

pub type NidHandler = Arc<dyn Fn(&mut NidCall<'_, '_>) -> wasmtime::Result<()> + Send + Sync>;

#[derive(Clone, Default)]
pub struct NidTable {
    pub handlers: HashMap<String, NidHandler>,
}

impl NidTable {
    pub fn with(mut self, nid: &str, handler: impl Fn(&mut NidCall<'_, '_>) -> wasmtime::Result<()> + Send + Sync + 'static) -> Self {
        self.handlers.insert(nid.to_string(), Arc::new(handler));
        self
    }
}

pub struct HostState {
    memory: Option<Memory>,
    dispatch: Option<TypedFunc<i64, ()>>,
    tls: TlsLayout,
    heap_next: u64,
    tsc: u64,
    unwind_images: Vec<super::unwind::UnwindImage>,
    pub output: Vec<String>,
}

#[derive(Clone, Default)]
struct TlsLayout {
    offsets: Vec<i64>,
    thread_pointer: u64,
}

pub struct Machine {
    store: Store<HostState>,
    memory: Memory,
    dispatch: TypedFunc<i64, ()>,
    pub metadata: Metadata,
    pub bases: Vec<u64>,
}

pub struct NidCall<'a, 'b> {
    caller: &'a mut Caller<'b, HostState>,
    nonlocal: bool,
}

impl NidCall<'_, '_> {
    fn memory(&self) -> Memory {
        self.caller.data().memory.unwrap()
    }

    pub fn read(&mut self, address: u64, length: usize) -> Vec<u8> {
        let memory = self.memory();
        memory.data(&self.caller)[address as usize..address as usize + length].to_vec()
    }

    pub fn write(&mut self, address: u64, bytes: &[u8]) {
        let memory = self.memory();
        memory.data_mut(&mut *self.caller)[address as usize..address as usize + bytes.len()].copy_from_slice(bytes);
    }

    pub fn read_u64(&mut self, address: u64) -> u64 {
        u64::from_le_bytes(self.read(address, 8).try_into().unwrap())
    }

    pub fn write_u64(&mut self, address: u64, value: u64) {
        self.write(address, &value.to_le_bytes());
    }

    pub fn read_c_string(&mut self, address: u64) -> String {
        let memory = self.memory();
        let data = memory.data(&self.caller);
        let start = address as usize;
        let end = start + data[start..].iter().position(|&byte| byte == 0).unwrap();
        String::from_utf8_lossy(&data[start..end]).into_owned()
    }

    pub fn gpr(&mut self, index: u32) -> u64 {
        self.read_u64(CTX_ADDR + abi::cpu::gpr(index))
    }

    pub fn set_gpr(&mut self, index: u32, value: u64) {
        self.write_u64(CTX_ADDR + abi::cpu::gpr(index), value);
    }

    pub fn arg(&mut self, index: usize) -> u64 {
        if index < ARGUMENT_REGISTERS.len() {
            return self.gpr(ARGUMENT_REGISTERS[index]);
        }
        let rsp = self.gpr(4);
        self.read_u64(rsp + 8 + 8 * (index - ARGUMENT_REGISTERS.len()) as u64)
    }

    pub fn set_return(&mut self, value: u64) {
        self.set_gpr(0, value);
    }

    pub fn xmm_f64(&mut self, index: u32) -> f64 {
        f64::from_bits(self.read_u64(CTX_ADDR + abi::cpu::ymm(index)))
    }

    pub fn set_xmm_f64(&mut self, index: u32, value: f64) {
        self.write_u64(CTX_ADDR + abi::cpu::ymm(index), value.to_bits());
    }

    pub fn output(&mut self, text: String) {
        self.caller.data_mut().output.push(text);
    }

    pub fn allocate(&mut self, size: u64) -> u64 {
        let state = self.caller.data_mut();
        let address = (state.heap_next + 15) & !15;
        state.heap_next = address + size;
        address
    }

    pub fn transfer(&mut self, rip: u64, rsp: u64) {
        self.write_u64(CTX_ADDR + abi::cpu::RIP, rip);
        self.set_gpr(4, rsp);
        self.nonlocal = true;
    }

    pub fn guest_call(&mut self, target: u64, args: &[u64]) -> wasmtime::Result<u64> {
        guest_call(&mut *self.caller, target, args)
    }

    pub fn throw(&mut self, exception: u64) -> wasmtime::Result<()> {
        let mut gpr = [0u64; 16];
        for (index, value) in gpr.iter_mut().enumerate() {
            *value = self.gpr(index as u32);
        }
        let memory = self.memory();
        let images = self.caller.data().unwind_images.clone();
        let data = memory.data(&*self.caller);
        let read = |address: u64, length: usize| data[address as usize..address as usize + length].to_vec();
        let resume = super::unwind::find_landing_pad(&images, gpr, exception, &read).map_err(fail)?;
        for (index, value) in resume.gpr.iter().enumerate() {
            self.set_gpr(index as u32, *value);
        }
        self.transfer(resume.rip, resume.gpr[4]);
        Ok(())
    }

    pub fn tls_block(&mut self, module_id: u64) -> u64 {
        let tls = &self.caller.data().tls;
        (tls.thread_pointer as i64 + tls.offsets[module_id as usize - 1]) as u64
    }
}

fn read_ctx(memory: &Memory, store: impl wasmtime::AsContext<Data = HostState>, offset: u64) -> u64 {
    let start = (CTX_ADDR + offset) as usize;
    u64::from_le_bytes(memory.data(store.as_context())[start..start + 8].try_into().unwrap())
}

fn write_ctx(memory: &Memory, mut store: impl wasmtime::AsContextMut<Data = HostState>, offset: u64, value: u64) {
    let start = (CTX_ADDR + offset) as usize;
    memory.data_mut(store.as_context_mut())[start..start + 8].copy_from_slice(&value.to_le_bytes());
}

fn guest_call(caller: &mut Caller<'_, HostState>, target: u64, args: &[u64]) -> wasmtime::Result<u64> {
    let memory = caller.data().memory.unwrap();
    let dispatch = caller.data().dispatch.clone().unwrap();
    let size = abi::cpu::SIZE as usize;
    let saved: Vec<u8> = memory.data(&*caller)[CTX_ADDR as usize..CTX_ADDR as usize + size].to_vec();
    for (index, &value) in args.iter().enumerate() {
        write_ctx(&memory, &mut *caller, abi::cpu::gpr(ARGUMENT_REGISTERS[index]), value);
    }
    let rsp = (read_ctx(&memory, &*caller, abi::cpu::gpr(4)) - 128) & !15;
    let rsp = rsp - 8;
    memory.data_mut(&mut *caller)[rsp as usize..rsp as usize + 8].copy_from_slice(&GUEST_CALL_SENTINEL.to_le_bytes());
    write_ctx(&memory, &mut *caller, abi::cpu::gpr(4), rsp);
    write_ctx(&memory, &mut *caller, abi::cpu::RIP, target);
    write_ctx(&memory, &mut *caller, abi::cpu::STOP_RIP, GUEST_CALL_SENTINEL);
    dispatch.call(&mut *caller, CTX_ADDR as i64)?;
    let result = read_ctx(&memory, &*caller, abi::cpu::gpr(0));
    memory.data_mut(&mut *caller)[CTX_ADDR as usize..CTX_ADDR as usize + size].copy_from_slice(&saved);
    Ok(result)
}

fn fail(message: String) -> wasmtime::Error {
    wasmtime::Error::msg(message)
}

impl Machine {
    pub fn new(wasm: &[u8], nids: NidTable) -> Self {
        let mut config = Config::new();
        config.wasm_memory64(true).wasm_tail_call(true).wasm_simd(true).wasm_relaxed_simd(true).wasm_multi_value(true).wasm_threads(true);
        let engine = Engine::new(&config).unwrap();
        let module = Module::new(&engine, wasm).unwrap();
        let section = custom_section(wasm, abi::names::CUSTOM_SECTION).expect("game.wasm has no image metadata");
        let metadata = meta::decode(section).unwrap();

        let state =
            HostState { memory: None, dispatch: None, tls: TlsLayout::default(), heap_next: HEAP_BASE, tsc: 0, unwind_images: Vec::new(), output: Vec::new() };
        let mut store = Store::new(&engine, state);
        let pages = MEMORY_SIZE / 0x10000;
        let memory = Memory::new(&mut store, MemoryType::new64(pages, Some(pages))).unwrap();
        store.data_mut().memory = Some(memory);

        let mut next_base = IMAGE_REGION;
        let bases: Vec<u64> = metadata
            .images
            .iter()
            .map(|image| {
                let lowest = image.segments.iter().map(|segment| segment.vaddr).min().unwrap_or(0);
                if lowest != 0 {
                    return 0;
                }
                let base = next_base;
                next_base += (image.span.max(IMAGE_STRIDE) + 0xffff) & !0xffff;
                base
            })
            .collect();
        assert!(next_base <= TLS_AREA, "guest images overlap the TLS area");
        load_images(&mut store, &memory, &metadata, &bases);
        store.data_mut().unwind_images = metadata
            .images
            .iter()
            .zip(&bases)
            .map(|(image, &base)| super::unwind::UnwindImage {
                base,
                span: image.span,
                eh_frame_hdr: image.phdrs.iter().find(|phdr| phdr.kind == 0x6474_e550).map(|phdr| phdr.vaddr),
            })
            .collect();

        let mut linker = Linker::new(&engine);
        linker.define(&store, "env", abi::names::MEMORY, memory).unwrap();
        for (index, &base) in bases.iter().enumerate() {
            let global = Global::new(&mut store, GlobalType::new(ValType::I64, Mutability::Const), Val::I64(base as i64)).unwrap();
            linker.define(&store, "env", &format!("{}{index}", abi::names::IMAGE_BASE_PREFIX), global).unwrap();
        }
        for (name, value) in [(abi::names::STUB_BASE, STUB_BASE), (abi::names::ADDR_MAP, ADDR_MAP)] {
            let global = Global::new(&mut store, GlobalType::new(ValType::I64, Mutability::Const), Val::I64(value as i64)).unwrap();
            linker.define(&store, "env", name, global).unwrap();
        }
        define_runtime_imports(&mut linker);
        for import in module.imports() {
            if import.module() != abi::names::NID_MODULE {
                continue;
            }
            let nid = import.name().to_string();
            let handler = nids.handlers.get(&nid).cloned();
            let ty = import.ty().func().unwrap().clone();
            linker
                .func_new(abi::names::NID_MODULE, &nid.clone(), ty, move |mut caller, params, _| {
                    let Some(handler) = &handler else {
                        return Err(fail(format!("NID {nid} is not provided by the test host")));
                    };
                    assert_eq!(params[0].unwrap_i64() as u64, CTX_ADDR);
                    let mut call = NidCall { caller: &mut caller, nonlocal: false };
                    handler(&mut call)?;
                    if !call.nonlocal {
                        let rsp = call.gpr(4);
                        let rip = call.read_u64(rsp);
                        call.write_u64(CTX_ADDR + abi::cpu::RIP, rip);
                        call.set_gpr(4, rsp + 8);
                    }
                    Ok(())
                })
                .unwrap();
        }

        let instance = linker.instantiate(&mut store, &module).unwrap();
        let version = instance.get_typed_func::<(), i32>(&mut store, abi::names::ABI_VERSION_EXPORT).unwrap();
        assert_eq!(version.call(&mut store, ()).unwrap() as u32, abi::ABI_VERSION);
        let dispatch = instance.get_typed_func::<i64, ()>(&mut store, abi::names::DISPATCH).unwrap();
        store.data_mut().dispatch = Some(dispatch.clone());
        Self { store, memory, dispatch, metadata, bases }
    }

    pub fn write(&mut self, address: u64, bytes: &[u8]) {
        self.memory.data_mut(&mut self.store)[address as usize..address as usize + bytes.len()].copy_from_slice(bytes);
    }

    pub fn read(&self, address: u64, length: usize) -> Vec<u8> {
        self.memory.data(&self.store)[address as usize..address as usize + length].to_vec()
    }

    pub fn read_u64(&self, address: u64) -> u64 {
        u64::from_le_bytes(self.read(address, 8).try_into().unwrap())
    }

    pub fn set_state(&mut self, gpr: &[u64; 16], rflags: u64, ymm: &[[u8; 32]; 16]) {
        for (index, value) in gpr.iter().enumerate() {
            write_ctx(&self.memory, &mut self.store, abi::cpu::gpr(index as u32), *value);
        }
        write_ctx(&self.memory, &mut self.store, abi::cpu::RFLAGS, rflags);
        self.write(CTX_ADDR + abi::cpu::MXCSR, &0x1f80u32.to_le_bytes());
        self.write(CTX_ADDR + abi::cpu::X87_FCW, &0x037fu16.to_le_bytes());
        for (index, value) in ymm.iter().enumerate() {
            self.write(CTX_ADDR + abi::cpu::ymm(index as u32), value);
        }
    }

    pub fn state(&self) -> ([u64; 16], u64, [[u8; 32]; 16]) {
        let mut gpr = [0; 16];
        for (index, value) in gpr.iter_mut().enumerate() {
            *value = read_ctx(&self.memory, &self.store, abi::cpu::gpr(index as u32));
        }
        let mut ymm = [[0; 32]; 16];
        for (index, value) in ymm.iter_mut().enumerate() {
            value.copy_from_slice(&self.read(CTX_ADDR + abi::cpu::ymm(index as u32), 32));
        }
        (gpr, read_ctx(&self.memory, &self.store, abi::cpu::RFLAGS), ymm)
    }

    pub fn output(&self) -> &[String] {
        &self.store.data().output
    }

    pub fn run(&mut self, rip: u64, stop: u64) -> wasmtime::Result<()> {
        write_ctx(&self.memory, &mut self.store, abi::cpu::RIP, rip);
        write_ctx(&self.memory, &mut self.store, abi::cpu::STOP_RIP, stop);
        self.dispatch.call(&mut self.store, CTX_ADDR as i64)
    }

    pub fn call_export(&mut self, image: usize, nid: &str, args: &[u64]) -> wasmtime::Result<u64> {
        let export = self.metadata.images[image].exports.iter().find(|export| export.nid == nid).unwrap_or_else(|| panic!("no export {nid}"));
        let target = self.bases[image] + export.vaddr;
        self.call_address(target, args)
    }

    pub fn call_address(&mut self, target: u64, args: &[u64]) -> wasmtime::Result<u64> {
        if read_ctx(&self.memory, &self.store, abi::cpu::gpr(4)) == 0 {
            write_ctx(&self.memory, &mut self.store, abi::cpu::gpr(4), GUEST_STACK_TOP);
            let thread_pointer = self.store.data().tls.thread_pointer;
            write_ctx(&self.memory, &mut self.store, abi::cpu::FS_BASE, thread_pointer);
        }
        let dispatch = self.dispatch.clone();
        let memory = self.memory;
        let saved = self.read(CTX_ADDR, abi::cpu::SIZE as usize);
        for (index, &value) in args.iter().enumerate() {
            write_ctx(&memory, &mut self.store, abi::cpu::gpr(ARGUMENT_REGISTERS[index]), value);
        }
        let rsp = (read_ctx(&memory, &self.store, abi::cpu::gpr(4)) - 128) & !15;
        let rsp = rsp - 8;
        self.write(rsp, &GUEST_CALL_SENTINEL.to_le_bytes());
        write_ctx(&memory, &mut self.store, abi::cpu::gpr(4), rsp);
        write_ctx(&memory, &mut self.store, abi::cpu::RIP, target);
        write_ctx(&memory, &mut self.store, abi::cpu::STOP_RIP, GUEST_CALL_SENTINEL);
        dispatch.call(&mut self.store, CTX_ADDR as i64)?;
        let result = read_ctx(&memory, &self.store, abi::cpu::gpr(0));
        let rsp_after = read_ctx(&memory, &self.store, abi::cpu::gpr(4));
        assert_eq!(rsp_after, rsp + 8, "guest call returned with an unbalanced stack");
        self.write(CTX_ADDR, &saved);
        Ok(result)
    }

    pub fn initialize(&mut self) -> wasmtime::Result<()> {
        for image in (0..self.metadata.images.len()).rev() {
            let base = self.bases[image];
            let meta = self.metadata.images[image].clone();
            if let Some(init) = meta.init {
                self.call_address(base + init, &[])?;
            }
            if let Some((vaddr, count)) = meta.init_array {
                for index in 0..count as u64 {
                    let target = self.read_u64(base + vaddr + 8 * index);
                    self.call_address(target, &[])?;
                }
            }
        }
        Ok(())
    }
}

fn define_runtime_imports(linker: &mut Linker<HostState>) {
    let module = abi::names::RUNTIME_MODULE;
    linker
        .func_wrap(module, abi::names::CPUID, |mut caller: Caller<'_, HostState>, ctx: i64| {
            let memory = caller.data().memory.unwrap();
            let leaf = read_ctx(&memory, &caller, abi::cpu::gpr(0)) as u32;
            let (eax, ebx, ecx, edx) = match leaf {
                0 => (0x10, 0x6874_7541, 0x444d_4163, 0x6974_6e65),
                _ => (0, 0, 0, 0),
            };
            assert_eq!(ctx as u64, CTX_ADDR);
            for (register, value) in [(0, eax), (3, ebx), (1, ecx), (2, edx)] {
                write_ctx(&memory, &mut caller, abi::cpu::gpr(register), value as u64);
            }
        })
        .unwrap();
    linker
        .func_wrap(module, abi::names::RDTSC, |mut caller: Caller<'_, HostState>, _ctx: i64| {
            let memory = caller.data().memory.unwrap();
            caller.data_mut().tsc += 1000;
            let tsc = caller.data().tsc;
            write_ctx(&memory, &mut caller, abi::cpu::gpr(0), tsc & 0xffff_ffff);
            write_ctx(&memory, &mut caller, abi::cpu::gpr(2), tsc >> 32);
        })
        .unwrap();
    linker
        .func_wrap(module, abi::names::CMPXCHG16B, |mut caller: Caller<'_, HostState>, _ctx: i64, address: i64| {
            let memory = caller.data().memory.unwrap();
            let address = address as usize;
            let current_low = u64::from_le_bytes(memory.data(&caller)[address..address + 8].try_into().unwrap());
            let current_high = u64::from_le_bytes(memory.data(&caller)[address + 8..address + 16].try_into().unwrap());
            let [rax, rdx, rbx, rcx] = [0, 2, 3, 1].map(|register| read_ctx(&memory, &caller, abi::cpu::gpr(register)));
            let mut rflags = read_ctx(&memory, &caller, abi::cpu::RFLAGS) & !0x40;
            if current_low == rax && current_high == rdx {
                memory.data_mut(&mut caller)[address..address + 8].copy_from_slice(&rbx.to_le_bytes());
                memory.data_mut(&mut caller)[address + 8..address + 16].copy_from_slice(&rcx.to_le_bytes());
                rflags |= 0x40;
            } else {
                write_ctx(&memory, &mut caller, abi::cpu::gpr(0), current_low);
                write_ctx(&memory, &mut caller, abi::cpu::gpr(2), current_high);
            }
            write_ctx(&memory, &mut caller, abi::cpu::RFLAGS, rflags);
        })
        .unwrap();
    linker
        .func_wrap(module, abi::names::UNRESOLVED_INDIRECT, |_: Caller<'_, HostState>, _ctx: i64, address: i64| -> wasmtime::Result<()> {
            Err(fail(format!("unresolved indirect branch to {:#x}", address as u64)))
        })
        .unwrap();
    linker
        .func_wrap(module, abi::names::UNSUPPORTED, |_: Caller<'_, HostState>, _ctx: i64, address: i64| -> wasmtime::Result<()> {
            Err(fail(format!("unsupported instruction at {:#x}", address as u64)))
        })
        .unwrap();
}

fn load_images(store: &mut Store<HostState>, memory: &Memory, metadata: &Metadata, bases: &[u64]) {
    let write = |store: &mut Store<HostState>, address: u64, bytes: &[u8]| {
        memory.data_mut(&mut *store)[address as usize..address as usize + bytes.len()].copy_from_slice(bytes);
    };
    for (image, &base) in metadata.images.iter().zip(bases) {
        for segment in &image.segments {
            write(store, base + segment.vaddr, &segment.data);
        }
    }
    write(store, STUB_BASE, &vec![0xcc; metadata.stubs.len() * abi::STUB_STRIDE as usize]);
    write(store, ADDR_MAP, &metadata.addr_map);

    let mut tls = TlsLayout { offsets: vec![0; metadata.images.len()], thread_pointer: 0 };
    let mut used: i64 = 0;
    for (index, image) in metadata.images.iter().enumerate() {
        if let Some(block) = &image.tls {
            used = (used + block.mem_size as i64 + block.align as i64 - 1) / block.align as i64 * block.align as i64;
            tls.offsets[index] = -used;
        }
    }
    tls.thread_pointer = (TLS_AREA + used as u64 + 63) & !63;
    write(store, tls.thread_pointer, &tls.thread_pointer.to_le_bytes());
    for (index, image) in metadata.images.iter().enumerate() {
        if let Some(block) = &image.tls {
            let start = (tls.thread_pointer as i64 + tls.offsets[index]) as u64;
            let init = memory.data(&*store)[(bases[index] + block.init_vaddr) as usize..(bases[index] + block.init_vaddr + block.init_size) as usize].to_vec();
            write(store, start, &init);
        }
    }

    for (index, image) in metadata.images.iter().enumerate() {
        let base = bases[index];
        for relocation in &image.relocations {
            let symbol = |target: u32| metadata.symbols[target as usize];
            let value = match relocation.kind {
                RelocKind::Relative => base.wrapping_add(relocation.addend as u64),
                RelocKind::GuestSymbol => {
                    let symbol = symbol(relocation.target);
                    bases[symbol.image as usize].wrapping_add(symbol.vaddr).wrapping_add(relocation.addend as u64)
                }
                RelocKind::ImportStub => (STUB_BASE + abi::STUB_STRIDE * relocation.target as u64).wrapping_add(relocation.addend as u64),
                RelocKind::HostData => (HOST_DATA + 64 * relocation.target as u64).wrapping_add(relocation.addend as u64),
                RelocKind::TlsModuleId => symbol(relocation.target).image as u64 + 1,
                RelocKind::TlsOffset => {
                    let symbol = symbol(relocation.target);
                    (tls.offsets[symbol.image as usize] + symbol.vaddr as i64 + relocation.addend) as u64
                }
                RelocKind::DtpOffset => symbol(relocation.target).vaddr.wrapping_add(relocation.addend as u64),
            };
            write(store, base + relocation.offset, &value.to_le_bytes());
        }
    }
    store.data_mut().tls = tls;
}

pub fn custom_section<'a>(wasm: &'a [u8], name: &str) -> Option<&'a [u8]> {
    for payload in wasmparser::Parser::new(0).parse_all(wasm) {
        if let Ok(wasmparser::Payload::CustomSection(reader)) = payload
            && reader.name() == name
        {
            return Some(reader.data());
        }
    }
    None
}
