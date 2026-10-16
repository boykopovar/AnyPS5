mod body;
mod function;
mod helpers;
mod ops;

pub use ops::{is_string, is_trap};

use std::borrow::Cow;

use wasm_encoder::{
    CodeSection, ConstExpr, CustomSection, ElementSection, Elements, EntityType, ExportKind, ExportSection, Function, FunctionSection, GlobalType,
    ImportSection, MemoryType, Module, NameMap, NameSection, RefType, TableSection, TableType, TypeSection, ValType,
};

use crate::abi::{self, addr_map};
use crate::discover::Discovery;
use crate::flags::Liveness;
use crate::meta::{self, ExportKind as MetaExportKind, ExportMeta, ImageMeta, Metadata, PhdrMeta, SegmentMeta, TlsMeta};
use crate::program::Program;
use crate::{Result, TranslateOptions};

use body::{Body, mem};

pub const TYPE_CTX: u32 = 0;
pub const TYPE_CTX_ADDR: u32 = 1;
pub const TYPE_VERSION: u32 = 2;
pub const TYPE_LOOKUP: u32 = 3;
pub const TYPE_BINARY: u32 = 4;
pub const TYPE_DIVIDE: u32 = 5;
pub const TYPE_X80_LOAD: u32 = 6;
pub const TYPE_X80_STORE: u32 = 7;
pub const TYPE_SCALBN: u32 = 8;
pub const TYPE_PCMPSTR: u32 = 9;

pub struct Layout {
    pub cpuid: u32,
    pub rdtsc: u32,
    pub cmpxchg16b: u32,
    pub unresolved: u32,
    pub unsupported: u32,
    pub nid_base: u32,
    pub guest_base: u32,
    pub stub_base: u32,
    pub lookup: u32,
    pub dispatch: u32,
    pub version: u32,
    pub umulh: u32,
    pub smulh: u32,
    pub udiv128: u32,
    pub sdiv128: u32,
    pub x80_load: u32,
    pub x80_store: u32,
    pub scalbn: u32,
    pub pcmpstr: u32,
    pub stub_base_global: u32,
    pub addr_map_global: u32,
}

impl Layout {
    fn new(imports: u32, functions: u32, stubs: u32, images: u32) -> Self {
        let nid_base = 5;
        let guest_base = nid_base + imports;
        let stub_base = guest_base + functions;
        let lookup = stub_base + stubs;
        Self {
            cpuid: 0,
            rdtsc: 1,
            cmpxchg16b: 2,
            unresolved: 3,
            unsupported: 4,
            nid_base,
            guest_base,
            stub_base,
            lookup,
            dispatch: lookup + 1,
            version: lookup + 2,
            umulh: lookup + 3,
            smulh: lookup + 4,
            udiv128: lookup + 5,
            sdiv128: lookup + 6,
            x80_load: lookup + 7,
            x80_store: lookup + 8,
            scalbn: lookup + 9,
            pcmpstr: lookup + 10,
            stub_base_global: images,
            addr_map_global: images + 1,
        }
    }

    pub fn image_base_global(&self, image: u32) -> u32 {
        image
    }
}

pub struct Context<'a> {
    pub program: &'a Program,
    pub discovery: &'a Discovery,
    pub options: &'a TranslateOptions,
    pub layout: Layout,
    pub unsupported: std::cell::RefCell<Vec<crate::report::Unsupported>>,
}

pub fn emit_module(
    program: &Program,
    discovery: &Discovery,
    liveness: &[Liveness],
    options: &TranslateOptions,
) -> Result<(Vec<u8>, Vec<crate::report::Unsupported>)> {
    let images = program.images.len() as u32;
    let stubs = program.imports.len() as u32;
    let functions = discovery.functions.len() as u32;
    let context =
        Context { program, discovery, options, layout: Layout::new(program.imports.len() as u32, functions, stubs, images), unsupported: Default::default() };
    let layout = &context.layout;

    let mut types = TypeSection::new();
    types.ty().function([ValType::I64], []);
    types.ty().function([ValType::I64, ValType::I64], []);
    types.ty().function([], [ValType::I32]);
    types.ty().function([ValType::I64, ValType::I64], [ValType::I32]);
    types.ty().function([ValType::I64, ValType::I64], [ValType::I64]);
    types.ty().function([ValType::I64, ValType::I64, ValType::I64], [ValType::I64, ValType::I64]);
    types.ty().function([ValType::I64], [ValType::F64]);
    types.ty().function([ValType::I64, ValType::F64], []);
    types.ty().function([ValType::F64, ValType::I32], [ValType::F64]);
    types.ty().function([ValType::V128, ValType::V128, ValType::I32, ValType::I32, ValType::I32], [ValType::I32]);

    let mut imports = ImportSection::new();
    let runtime = abi::names::RUNTIME_MODULE;
    imports.import(runtime, abi::names::CPUID, EntityType::Function(TYPE_CTX));
    imports.import(runtime, abi::names::RDTSC, EntityType::Function(TYPE_CTX));
    imports.import(runtime, abi::names::CMPXCHG16B, EntityType::Function(TYPE_CTX_ADDR));
    imports.import(runtime, abi::names::UNRESOLVED_INDIRECT, EntityType::Function(TYPE_CTX_ADDR));
    imports.import(runtime, abi::names::UNSUPPORTED, EntityType::Function(TYPE_CTX_ADDR));
    for nid in &program.imports {
        imports.import(abi::names::NID_MODULE, nid, EntityType::Function(TYPE_CTX));
    }
    let constant_i64 = EntityType::Global(GlobalType { val_type: ValType::I64, mutable: false, shared: false });
    for image in 0..images {
        imports.import("env", &format!("{}{image}", abi::names::IMAGE_BASE_PREFIX), constant_i64);
    }
    imports.import("env", abi::names::STUB_BASE, constant_i64);
    imports.import("env", abi::names::ADDR_MAP, constant_i64);
    let maximum = options.shared_memory.then_some(1 << 18);
    let memory = MemoryType { minimum: 1, maximum, memory64: true, shared: options.shared_memory, page_size_log2: None };
    imports.import("env", abi::names::MEMORY, EntityType::Memory(memory));

    let mut function_section = FunctionSection::new();
    let mut code = CodeSection::new();
    let mut names = NameMap::new();
    for (index, function) in discovery.functions.iter().enumerate() {
        function_section.function(TYPE_CTX);
        code.function(&function::emit(&context, function, &liveness[index])?);
        names.append(layout.guest_base + index as u32, &format!("g{}_{:016x}", function.image, function.start));
    }
    for (index, nid) in program.imports.iter().enumerate() {
        function_section.function(TYPE_CTX);
        let mut body = Body::new(1, &[]);
        body.sink().local_get(0).call(layout.nid_base + index as u32).end();
        code.function(&body.finish());
        names.append(layout.stub_base + index as u32, &format!("stub_{nid}"));
    }
    function_section.function(TYPE_LOOKUP);
    code.function(&lookup_function(&context));
    names.append(layout.lookup, "lookup");
    function_section.function(TYPE_CTX);
    code.function(&dispatch_function(&context));
    names.append(layout.dispatch, abi::names::DISPATCH);
    function_section.function(TYPE_VERSION);
    let mut version = Body::new(0, &[]);
    version.sink().i32_const(abi::ABI_VERSION as i32).end();
    code.function(&version.finish());
    names.append(layout.version, abi::names::ABI_VERSION_EXPORT);
    for (index, function, ty, name) in [
        (layout.umulh, helpers::umulh(), TYPE_BINARY, "umulh"),
        (layout.smulh, helpers::smulh(layout.umulh), TYPE_BINARY, "smulh"),
        (layout.udiv128, helpers::udiv128(), TYPE_DIVIDE, "udiv128"),
        (layout.sdiv128, helpers::sdiv128(layout.udiv128), TYPE_DIVIDE, "sdiv128"),
        (layout.x80_load, helpers::x80_load(layout.scalbn), TYPE_X80_LOAD, "x80_load"),
        (layout.x80_store, helpers::x80_store(), TYPE_X80_STORE, "x80_store"),
        (layout.scalbn, helpers::scalbn(), TYPE_SCALBN, "scalbn"),
        (layout.pcmpstr, helpers::pcmpstr(), TYPE_PCMPSTR, "pcmpstr"),
    ] {
        function_section.function(ty);
        code.function(&function);
        names.append(index, name);
    }
    for (index, nid) in program.imports.iter().enumerate() {
        names.append(layout.nid_base + index as u32, &format!("nid_{nid}"));
    }

    let table_size = (functions + stubs) as u64;
    let mut tables = TableSection::new();
    tables.table(TableType { element_type: RefType::FUNCREF, table64: false, minimum: table_size, maximum: Some(table_size), shared: false });

    let mut exports = ExportSection::new();
    exports.export(abi::names::DISPATCH, ExportKind::Func, layout.dispatch);
    exports.export(abi::names::ABI_VERSION_EXPORT, ExportKind::Func, layout.version);

    let mut elements = ElementSection::new();
    let members: Vec<u32> = (0..functions).map(|index| layout.guest_base + index).chain((0..stubs).map(|index| layout.stub_base + index)).collect();
    elements.active(Some(0), &ConstExpr::i32_const(0), Elements::Functions(Cow::Owned(members)));

    let metadata = metadata(program, discovery);
    let mut module = Module::new();
    module.section(&types);
    module.section(&imports);
    module.section(&function_section);
    module.section(&tables);
    module.section(&exports);
    module.section(&elements);
    module.section(&code);
    let mut name_section = NameSection::new();
    name_section.functions(&names);
    module.section(&name_section);
    module.section(&CustomSection { name: Cow::Borrowed(abi::names::CUSTOM_SECTION), data: Cow::Owned(meta::encode(&metadata)) });
    Ok((module.finish(), context.unsupported.into_inner()))
}

fn lookup_function(context: &Context<'_>) -> Function {
    let layout = &context.layout;
    let program = context.program;
    let mut body = Body::new(2, &[ValType::I64, ValType::I64, ValType::I64, ValType::I64, ValType::I64]);
    let mut sink = body.sink();
    sink.block(wasm_encoder::BlockType::Empty);
    let regions = program
        .images
        .iter()
        .enumerate()
        .map(|(index, image)| (index as u32, layout.image_base_global(index as u32), image.span()))
        .chain(std::iter::once((program.images.len() as u32, layout.stub_base_global, program.imports.len() as u64 * abi::STUB_STRIDE)));
    for (index, base, span) in regions {
        sink.local_get(1).global_get(base).i64_sub().local_tee(3).i64_const(span as i64).i64_lt_u().if_(wasm_encoder::BlockType::Empty);
        sink.local_get(3).i64_const(addr_map::key(index, 0) as i64).i64_or().local_set(2).br(1);
        sink.end();
    }
    sink.local_get(0).local_get(1).call(layout.unresolved).unreachable();
    sink.end();
    sink.global_get(layout.addr_map_global).i64_load(mem(0)).local_set(6);
    sink.local_get(2).i64_const(0x9e37_79b9_7f4a_7c15u64 as i64).i64_mul().local_tee(4).local_get(4).i64_const(32).i64_shr_u().i64_xor();
    sink.local_get(6).i64_and().local_set(4);
    sink.loop_(wasm_encoder::BlockType::Empty);
    sink.global_get(layout.addr_map_global).local_get(4).i64_const(addr_map::ENTRY_SIZE as i64).i64_mul().i64_add();
    sink.i64_const(addr_map::HEADER_SIZE as i64).i64_add().local_tee(5).i64_load(mem(0)).local_get(2).i64_eq();
    sink.if_(wasm_encoder::BlockType::Empty).local_get(5).i32_load(mem(8)).return_().end();
    sink.local_get(5).i64_load(mem(0)).i64_const(addr_map::EMPTY_KEY as i64).i64_eq();
    sink.if_(wasm_encoder::BlockType::Empty).local_get(0).local_get(1).call(layout.unresolved).unreachable().end();
    sink.local_get(4).i64_const(1).i64_add().local_get(6).i64_and().local_set(4).br(0);
    sink.end();
    sink.unreachable().end();
    body.finish()
}

fn dispatch_function(context: &Context<'_>) -> Function {
    let layout = &context.layout;
    let mut body = Body::new(1, &[]);
    let mut sink = body.sink();
    sink.loop_(wasm_encoder::BlockType::Empty);
    sink.local_get(0).i64_load(mem(abi::cpu::RIP)).local_get(0).i64_load(mem(abi::cpu::STOP_RIP)).i64_eq();
    sink.if_(wasm_encoder::BlockType::Empty).return_().end();
    sink.local_get(0).local_get(0).local_get(0).i64_load(mem(abi::cpu::RIP)).call(layout.lookup);
    sink.call_indirect(0, TYPE_CTX).br(0);
    sink.end();
    sink.end();
    body.finish()
}

fn metadata(program: &Program, discovery: &Discovery) -> Metadata {
    let mut images = Vec::new();
    for (index, image) in program.images.iter().enumerate() {
        let exports = image
            .symbols
            .iter()
            .filter_map(|symbol| {
                let kind = match symbol.kind {
                    crate::image::SymbolKind::Function => MetaExportKind::Function,
                    _ => MetaExportKind::Data,
                };
                Some(ExportMeta { nid: symbol.name.clone(), vaddr: symbol.value?, kind })
            })
            .collect();
        images.push(ImageMeta {
            name: image.name.clone(),
            span: image.span(),
            align: image.align(),
            segments: image
                .segments
                .iter()
                .map(|segment| SegmentMeta { vaddr: segment.vaddr, memsz: segment.memsz, flags: segment.flags, data: segment.data.clone() })
                .collect(),
            phdrs: image.phdrs.iter().map(|phdr| PhdrMeta { kind: phdr.kind, flags: phdr.flags, vaddr: phdr.vaddr, memsz: phdr.memsz }).collect(),
            tls: image.tls.map(|tls| TlsMeta { init_vaddr: tls.init_vaddr, init_size: tls.init_size, mem_size: tls.mem_size, align: tls.align }),
            entry: image.entry,
            init: image.init,
            fini: image.fini,
            init_array: image.init_array,
            fini_array: image.fini_array,
            needed: image.needed.clone(),
            exports,
            relocations: program.relocations[index].clone(),
        });
    }
    Metadata {
        images,
        stubs: program.imports.clone(),
        host_data: program.host_data.clone(),
        symbols: program.symbols.clone(),
        addr_map: address_map(program, discovery),
    }
}

fn address_map(program: &Program, discovery: &Discovery) -> Vec<u8> {
    let mut entries: Vec<(u64, u32)> = Vec::new();
    for (index, function) in discovery.functions.iter().enumerate() {
        for &entry in &function.entries {
            entries.push((addr_map::key(function.image, entry), index as u32));
        }
    }
    let functions = discovery.functions.len() as u32;
    for stub in 0..program.imports.len() as u64 {
        entries.push((addr_map::key(program.images.len() as u32, stub * abi::STUB_STRIDE), functions + stub as u32));
    }
    let capacity = (entries.len() * 2).next_power_of_two().max(16);
    let mask = capacity as u64 - 1;
    let mut slots = vec![(addr_map::EMPTY_KEY, 0u32); capacity];
    for (key, index) in entries {
        let mut slot = addr_map::hash(key) & mask;
        while slots[slot as usize].0 != addr_map::EMPTY_KEY {
            slot = (slot + 1) & mask;
        }
        slots[slot as usize] = (key, index);
    }
    let mut bytes = Vec::with_capacity(16 + 16 * capacity);
    bytes.extend_from_slice(&mask.to_le_bytes());
    bytes.extend_from_slice(&(capacity as u64).to_le_bytes());
    for (key, index) in slots {
        bytes.extend_from_slice(&key.to_le_bytes());
        bytes.extend_from_slice(&index.to_le_bytes());
        bytes.extend_from_slice(&0u32.to_le_bytes());
    }
    bytes
}
