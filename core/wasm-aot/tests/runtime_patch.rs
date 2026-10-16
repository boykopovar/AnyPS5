use anyps5_wasm_aot::runtime_patch::{GUEST_INDIRECT_EXPORT, route_guest_calls};
use wasm_encoder::{
    CodeSection, ConstExpr, ElementSection, Elements, ExportKind, ExportSection, Function, FunctionSection, Module, RefType, TableSection, TableType, TypeSection,
    ValType,
};
use wasmtime::{Engine, Instance, Store};

const INTEGER_TYPE: u32 = 0;
const APPLY_TYPE: u32 = 1;
const BRIDGE_TYPE: u32 = 2;
const FLOAT_TYPE: u32 = 3;
const SCALE_TYPE: u32 = 4;

fn host_module() -> Vec<u8> {
    let mut types = TypeSection::new();
    types.ty().function([ValType::I32], [ValType::I32]);
    types.ty().function([ValType::I64, ValType::I32], [ValType::I32]);
    types.ty().function([ValType::I64, ValType::I32, ValType::I64, ValType::I64, ValType::I64, ValType::I64, ValType::I64, ValType::I64, ValType::I64, ValType::I64], [ValType::I64]);
    types.ty().function([ValType::F64, ValType::I32], [ValType::F64]);
    types.ty().function([ValType::I64, ValType::F64, ValType::I32], [ValType::F64]);

    let mut functions = FunctionSection::new();
    for ty in [INTEGER_TYPE, APPLY_TYPE, BRIDGE_TYPE, FLOAT_TYPE, SCALE_TYPE] {
        functions.function(ty);
    }

    let mut tables = TableSection::new();
    tables.table(TableType { element_type: RefType::FUNCREF, table64: true, minimum: 2, maximum: None, shared: false });

    let mut exports = ExportSection::new();
    exports.export("apply", ExportKind::Func, 1);
    exports.export(GUEST_INDIRECT_EXPORT, ExportKind::Func, 2);
    exports.export("scale", ExportKind::Func, 4);

    let mut elements = ElementSection::new();
    elements.active(Some(0), &ConstExpr::i64_const(0), Elements::Functions([0u32, 3].as_slice().into()));

    let mut code = CodeSection::new();
    let mut triple = Function::new([]);
    triple.instructions().local_get(0).i32_const(3).i32_mul().end();
    code.function(&triple);

    let mut apply = Function::new([]);
    apply.instructions().local_get(1).local_get(0).call_indirect(0, INTEGER_TYPE).end();
    code.function(&apply);

    let mut bridge = Function::new([]);
    let mut sink = bridge.instructions();
    sink.local_get(0).i64_const(0x1_0000_0000).i64_sub().i64_const(1000).i64_mul();
    sink.local_get(1).i64_extend_i32_u().i64_add();
    sink.local_get(2).i64_add().local_get(3).i64_add().end();
    code.function(&bridge);

    let mut half = Function::new([]);
    half.instructions().local_get(0).f64_const(0.5.into()).f64_mul().end();
    code.function(&half);

    let mut scale = Function::new([]);
    scale.instructions().local_get(1).local_get(2).local_get(0).call_indirect(0, FLOAT_TYPE).end();
    code.function(&scale);

    let mut module = Module::new();
    module.section(&types).section(&functions).section(&tables).section(&exports).section(&elements).section(&code);
    module.finish()
}

fn instantiate(wasm: &[u8]) -> (Store<()>, Instance) {
    let mut config = wasmtime::Config::new();
    config.wasm_memory64(true);
    let engine = Engine::new(&config).unwrap();
    let module = wasmtime::Module::new(&engine, wasm).unwrap();
    let mut store = Store::new(&engine, ());
    let instance = Instance::new(&mut store, &module, &[]).unwrap();
    (store, instance)
}

#[test]
fn table_indices_still_reach_host_functions() {
    let (mut store, instance) = instantiate(&route_guest_calls(&host_module()).unwrap());
    let apply = instance.get_typed_func::<(i64, i32), i32>(&mut store, "apply").unwrap();
    assert_eq!(apply.call(&mut store, (0, 7)).unwrap(), 21);
}

#[test]
fn guest_addresses_reach_the_bridge_with_a_descriptor() {
    let (mut store, instance) = instantiate(&route_guest_calls(&host_module()).unwrap());
    let apply = instance.get_typed_func::<(i64, i32), i32>(&mut store, "apply").unwrap();
    let integer_result = 1 << 20;
    assert_eq!(apply.call(&mut store, (0x1_0000_0002, 7)).unwrap(), 2000 + (1 + integer_result) + 7);
}

#[test]
fn float_arguments_are_marked_in_the_descriptor() {
    let (mut store, instance) = instantiate(&route_guest_calls(&host_module()).unwrap());
    let scale = instance.get_typed_func::<(i64, f64, i32), f64>(&mut store, "scale").unwrap();
    assert_eq!(scale.call(&mut store, (1, 3.0, 9)).unwrap(), 1.5);
    let float_result = 3 << 20;
    let descriptor = 2 | (1 << 4) | float_result;
    let expected = 1000 + descriptor as i64 + 3.0f64.to_bits() as i64 + 9;
    assert_eq!(scale.call(&mut store, (0x1_0000_0001, 3.0, 9)).unwrap().to_bits() as i64, expected);
}

#[test]
fn modules_without_the_bridge_export_are_rejected() {
    let mut module = Module::new();
    let mut types = TypeSection::new();
    types.ty().function([], []);
    module.section(&types);
    let error = route_guest_calls(&module.finish()).unwrap_err();
    assert!(error.to_string().contains(GUEST_INDIRECT_EXPORT), "{error}");
}

#[test]
fn patched_modules_validate() {
    let wasm = route_guest_calls(&host_module()).unwrap();
    wasmparser::Validator::new_with_features(wasmparser::WasmFeatures::all()).validate_all(&wasm).unwrap();
    let text = wasmprinter::print_bytes(&wasm).unwrap();
    assert_eq!(text.matches("call_indirect").count(), 2, "{text}");
}
