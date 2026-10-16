use std::collections::BTreeMap;

use wasm_encoder::{BlockType, Encode, Function, ValType};
use wasmparser::{BinaryReader, ExternalKind, FuncType, Operator, TypeRef};

use crate::{Error, Result};

pub const GUEST_INDIRECT_EXPORT: &str = "aps5_guest_indirect";
pub const GUEST_ADDRESS_FLOOR: i64 = 0x1_0000_0000;
pub const MAX_ARGUMENTS: usize = 8;
pub const UNSUPPORTED_COUNT: u32 = 0xF;

const TYPE_SECTION: u8 = 1;
const IMPORT_SECTION: u8 = 2;
const FUNCTION_SECTION: u8 = 3;
const TABLE_SECTION: u8 = 4;
const EXPORT_SECTION: u8 = 7;
const CODE_SECTION: u8 = 10;

struct Section<'a> {
    id: u8,
    offset: u64,
    contents: &'a [u8],
}

fn fail(error: wasmparser::BinaryReaderError) -> Error {
    Error::new(format!("runtime module: {error}"))
}

fn sections(wasm: &[u8]) -> Result<Vec<Section<'_>>> {
    if wasm.len() < 8 || &wasm[..4] != b"\0asm" {
        return Err(Error::new("runtime module is not a WebAssembly binary"));
    }
    let mut reader = BinaryReader::new(&wasm[8..], 8);
    let mut result = Vec::new();
    while !reader.eof() {
        let id = reader.read_u8().map_err(fail)?;
        let size = reader.read_var_u32().map_err(fail)? as usize;
        let offset = reader.original_position();
        let contents = reader.read_bytes(size).map_err(fail)?;
        result.push(Section { id, offset, contents });
    }
    Ok(result)
}

fn convert(ty: wasmparser::ValType) -> Option<ValType> {
    Some(match ty {
        wasmparser::ValType::I32 => ValType::I32,
        wasmparser::ValType::I64 => ValType::I64,
        wasmparser::ValType::F32 => ValType::F32,
        wasmparser::ValType::F64 => ValType::F64,
        wasmparser::ValType::V128 => ValType::V128,
        wasmparser::ValType::Ref(_) => return None,
    })
}

struct Layout {
    types: Vec<FuncType>,
    imported_functions: u32,
    defined_functions: u32,
    bridge: u32,
}

fn layout(sections: &[Section<'_>]) -> Result<Layout> {
    let mut types = Vec::new();
    let mut imported_functions = 0;
    let mut defined_types = Vec::new();
    let mut table64 = None;
    let mut bridge = None;
    for section in sections {
        let reader = BinaryReader::new(section.contents, section.offset);
        match section.id {
            TYPE_SECTION => {
                for group in wasmparser::TypeSectionReader::new(reader).map_err(fail)? {
                    let group = group.map_err(fail)?;
                    for sub_type in group.types() {
                        match &sub_type.composite_type.inner {
                            wasmparser::CompositeInnerType::Func(func) => types.push(func.clone()),
                            _ => return Err(Error::new("runtime module declares a non-function type")),
                        }
                    }
                }
            }
            IMPORT_SECTION => {
                for import in wasmparser::ImportSectionReader::new(reader).map_err(fail)?.into_imports() {
                    match import.map_err(fail)?.ty {
                        TypeRef::Func(_) => imported_functions += 1,
                        TypeRef::Table(table) if table64.is_none() => table64 = Some(table.table64),
                        _ => {}
                    }
                }
            }
            FUNCTION_SECTION => {
                for ty in wasmparser::FunctionSectionReader::new(reader).map_err(fail)? {
                    defined_types.push(ty.map_err(fail)?);
                }
            }
            TABLE_SECTION => {
                for table in wasmparser::TableSectionReader::new(reader).map_err(fail)? {
                    let table = table.map_err(fail)?;
                    if table64.is_none() {
                        table64 = Some(table.ty.table64);
                    }
                }
            }
            EXPORT_SECTION => {
                for export in wasmparser::ExportSectionReader::new(reader).map_err(fail)? {
                    let export = export.map_err(fail)?;
                    if export.name == GUEST_INDIRECT_EXPORT && export.kind == ExternalKind::Func {
                        bridge = Some(export.index);
                    }
                }
            }
            _ => {}
        }
    }
    let bridge = bridge.ok_or_else(|| Error::new(format!("runtime module does not export {GUEST_INDIRECT_EXPORT}")))?;
    if table64 != Some(true) {
        return Err(Error::new("runtime module needs a 64-bit function table to tell guest addresses from table indices"));
    }
    let bridge_type = bridge.checked_sub(imported_functions).and_then(|index| defined_types.get(index as usize)).ok_or_else(|| Error::new(format!("{GUEST_INDIRECT_EXPORT} is not a defined function")))?;
    let expected: Vec<wasmparser::ValType> =
        [wasmparser::ValType::I64, wasmparser::ValType::I32].into_iter().chain(std::iter::repeat_n(wasmparser::ValType::I64, MAX_ARGUMENTS)).collect();
    let actual = &types[*bridge_type as usize];
    if actual.params() != expected.as_slice() || actual.results() != [wasmparser::ValType::I64] {
        return Err(Error::new(format!("{GUEST_INDIRECT_EXPORT} must take (i64, i32, {MAX_ARGUMENTS} x i64) and return i64")));
    }
    Ok(Layout { types, imported_functions, defined_functions: defined_types.len() as u32, bridge })
}

pub fn descriptor(ty: &FuncType) -> u32 {
    let params = ty.params();
    let supported_param = |param: &wasmparser::ValType| matches!(param, wasmparser::ValType::I32 | wasmparser::ValType::I64 | wasmparser::ValType::F32 | wasmparser::ValType::F64);
    let result_kind = match ty.results() {
        [] => Some(0),
        [wasmparser::ValType::I32 | wasmparser::ValType::I64] => Some(1),
        [wasmparser::ValType::F32] => Some(2),
        [wasmparser::ValType::F64] => Some(3),
        _ => None,
    };
    let Some(result_kind) = result_kind.filter(|_| params.len() <= MAX_ARGUMENTS && params.iter().all(supported_param)) else {
        return UNSUPPORTED_COUNT;
    };
    let mut floats = 0;
    let mut singles = 0;
    for (index, param) in params.iter().enumerate() {
        match param {
            wasmparser::ValType::F32 => {
                floats |= 1 << index;
                singles |= 1 << index;
            }
            wasmparser::ValType::F64 => floats |= 1 << index,
            _ => {}
        }
    }
    params.len() as u32 | (floats << 4) | (singles << 12) | (result_kind << 20)
}

fn thunk(ty: &FuncType, type_index: u32, bridge: u32) -> Result<Function> {
    let index_local = ty.params().len() as u32;
    let block = match ty.results() {
        [] => BlockType::Empty,
        [result] => BlockType::Result(convert(*result).ok_or_else(|| Error::new("indirect call returns a reference"))?),
        _ => BlockType::FunctionType(type_index),
    };
    let descriptor = descriptor(ty);
    let mut function = Function::new([]);
    let mut sink = function.instructions();
    sink.local_get(index_local).i64_const(GUEST_ADDRESS_FLOOR).i64_lt_u().if_(block);
    for local in 0..index_local {
        sink.local_get(local);
    }
    sink.local_get(index_local).call_indirect(0, type_index);
    sink.else_();
    sink.local_get(index_local).i32_const(descriptor as i32);
    let mut passed = 0;
    if descriptor != UNSUPPORTED_COUNT {
        for (local, param) in ty.params().iter().enumerate() {
            sink.local_get(local as u32);
            match param {
                wasmparser::ValType::I32 => {
                    sink.i64_extend_i32_u();
                }
                wasmparser::ValType::F32 => {
                    sink.i32_reinterpret_f32().i64_extend_i32_u();
                }
                wasmparser::ValType::F64 => {
                    sink.i64_reinterpret_f64();
                }
                _ => {}
            }
            passed += 1;
        }
    }
    for _ in passed..MAX_ARGUMENTS {
        sink.i64_const(0);
    }
    sink.call(bridge);
    match ty.results() {
        [] => {
            sink.drop();
        }
        [wasmparser::ValType::I32] => {
            sink.i32_wrap_i64();
        }
        [wasmparser::ValType::F32] => {
            sink.i32_wrap_i64().f32_reinterpret_i32();
        }
        [wasmparser::ValType::F64] => {
            sink.f64_reinterpret_i64();
        }
        [wasmparser::ValType::I64] => {}
        _ => {
            sink.drop().unreachable();
        }
    }
    sink.end().end();
    Ok(function)
}

fn leb(value: u32, out: &mut Vec<u8>) {
    value.encode(out);
}

fn rewrite_body(body: wasmparser::FunctionBody<'_>, thunks: &BTreeMap<u32, u32>) -> Result<Vec<u8>> {
    let bytes = body.as_bytes();
    let start = body.range().start as usize;
    let mut operators = body.get_operators_reader().map_err(fail)?;
    let mut out = Vec::with_capacity(bytes.len());
    let mut copied = 0;
    while !operators.eof() {
        let (operator, offset) = operators.read_with_offset().map_err(fail)?;
        let (tail, type_index, table_index) = match operator {
            Operator::CallIndirect { type_index, table_index } => (false, type_index, table_index),
            Operator::ReturnCallIndirect { type_index, table_index } => (true, type_index, table_index),
            _ => continue,
        };
        if table_index != 0 {
            return Err(Error::new(format!("indirect call through table {table_index} at {offset:#x}")));
        }
        let end = operators.original_position() as usize - start;
        let begin = offset as usize - start;
        out.extend_from_slice(&bytes[copied..begin]);
        out.push(if tail { 0x12 } else { 0x10 });
        leb(thunks[&type_index], &mut out);
        copied = end;
    }
    out.extend_from_slice(&bytes[copied..]);
    Ok(out)
}

fn indirect_types(section: &Section<'_>) -> Result<Vec<u32>> {
    let mut found = std::collections::BTreeSet::new();
    for body in wasmparser::CodeSectionReader::new(BinaryReader::new(section.contents, section.offset)).map_err(fail)? {
        let mut operators = body.map_err(fail)?.get_operators_reader().map_err(fail)?;
        while !operators.eof() {
            match operators.read().map_err(fail)? {
                Operator::CallIndirect { type_index, .. } | Operator::ReturnCallIndirect { type_index, .. } => {
                    found.insert(type_index);
                }
                _ => {}
            }
        }
    }
    Ok(found.into_iter().collect())
}

fn append_vector(contents: &[u8], offset: u64, extra_count: u32, extra: &[u8]) -> Result<Vec<u8>> {
    let mut reader = BinaryReader::new(contents, offset);
    let count = reader.read_var_u32().map_err(fail)?;
    let rest = reader.read_bytes(reader.bytes_remaining()).map_err(fail)?;
    let mut out = Vec::new();
    leb(count + extra_count, &mut out);
    out.extend_from_slice(rest);
    out.extend_from_slice(extra);
    Ok(out)
}

fn write_section(out: &mut Vec<u8>, id: u8, contents: &[u8]) {
    out.push(id);
    leb(contents.len() as u32, out);
    out.extend_from_slice(contents);
}

pub fn route_guest_calls(wasm: &[u8]) -> Result<Vec<u8>> {
    let sections = sections(wasm)?;
    let layout = layout(&sections)?;
    let code = sections.iter().find(|section| section.id == CODE_SECTION);
    let indirect = match code {
        Some(code) => indirect_types(code)?,
        None => Vec::new(),
    };

    let mut new_types = Vec::new();
    let mut thunk_type_of = Vec::new();
    for &type_index in &indirect {
        let ty = layout.types.get(type_index as usize).ok_or_else(|| Error::new(format!("indirect call uses missing type {type_index}")))?;
        let mut params: Vec<ValType> = Vec::new();
        for &param in ty.params() {
            params.push(convert(param).ok_or_else(|| Error::new("indirect call passes a reference"))?);
        }
        params.push(ValType::I64);
        let mut results = Vec::new();
        for &result in ty.results() {
            results.push(convert(result).ok_or_else(|| Error::new("indirect call returns a reference"))?);
        }
        thunk_type_of.push(layout.types.len() as u32 + new_types.len() as u32);
        new_types.push((params, results));
    }
    let first_thunk = layout.imported_functions + layout.defined_functions;
    let thunks: BTreeMap<u32, u32> = indirect.iter().enumerate().map(|(position, &type_index)| (type_index, first_thunk + position as u32)).collect();

    let mut out = wasm[..8].to_vec();
    for section in &sections {
        match section.id {
            TYPE_SECTION => {
                let mut extra = Vec::new();
                for (params, results) in &new_types {
                    extra.push(0x60);
                    leb(params.len() as u32, &mut extra);
                    for param in params {
                        param.encode(&mut extra);
                    }
                    leb(results.len() as u32, &mut extra);
                    for result in results {
                        result.encode(&mut extra);
                    }
                }
                write_section(&mut out, TYPE_SECTION, &append_vector(section.contents, section.offset, new_types.len() as u32, &extra)?);
            }
            FUNCTION_SECTION => {
                let mut extra = Vec::new();
                for &ty in &thunk_type_of {
                    leb(ty, &mut extra);
                }
                write_section(&mut out, FUNCTION_SECTION, &append_vector(section.contents, section.offset, thunk_type_of.len() as u32, &extra)?);
            }
            CODE_SECTION => {
                let mut contents = Vec::new();
                leb(layout.defined_functions + indirect.len() as u32, &mut contents);
                for body in wasmparser::CodeSectionReader::new(BinaryReader::new(section.contents, section.offset)).map_err(fail)? {
                    let rewritten = rewrite_body(body.map_err(fail)?, &thunks)?;
                    leb(rewritten.len() as u32, &mut contents);
                    contents.extend_from_slice(&rewritten);
                }
                for &type_index in &indirect {
                    thunk(&layout.types[type_index as usize], type_index, layout.bridge)?.encode(&mut contents);
                }
                write_section(&mut out, CODE_SECTION, &contents);
            }
            id => write_section(&mut out, id, section.contents),
        }
    }
    Ok(out)
}
