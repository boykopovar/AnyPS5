mod common;

use anyps5_wasm_aot::{TranslateOptions, load_images, program::Program, translate};
use common::guest;
use common::host::{Machine, NidTable};

fn nids() -> NidTable {
    NidTable::default()
        .with("host_print_long", |call| {
            let value = call.arg(0) as i64;
            call.output(value.to_string());
            Ok(())
        })
        .with("host_add", |call| {
            let sum = call.arg(0).wrapping_add(call.arg(1));
            call.set_return(sum);
            Ok(())
        })
        .with("host_scale", |call| {
            let value = call.xmm_f64(0) * call.arg(0) as i64 as f64;
            call.set_xmm_f64(0, value);
            Ok(())
        })
        .with("host_apply", |call| {
            let (function, value) = (call.arg(0), call.arg(1));
            let result = call.guest_call(function, &[value])?;
            call.set_return(result);
            Ok(())
        })
        .with("host_save", |call| {
            let buffer = call.arg(0);
            let rsp = call.gpr(4);
            let return_address = call.read_u64(rsp);
            call.write_u64(buffer, return_address);
            call.write_u64(buffer + 8, rsp + 8);
            for (slot, register) in [3, 5, 12, 13, 14, 15].into_iter().enumerate() {
                let value = call.gpr(register);
                call.write_u64(buffer + 16 + 8 * slot as u64, value);
            }
            call.set_return(0);
            Ok(())
        })
        .with("host_restore", |call| {
            let (buffer, value) = (call.arg(0), call.arg(1));
            for (slot, register) in [3, 5, 12, 13, 14, 15].into_iter().enumerate() {
                let saved = call.read_u64(buffer + 16 + 8 * slot as u64);
                call.set_gpr(register, saved);
            }
            call.set_return(value);
            let (rip, rsp) = (call.read_u64(buffer), call.read_u64(buffer + 8));
            call.transfer(rip, rsp);
            Ok(())
        })
        .with("__tls_get_addr", |call| {
            let index = call.arg(0);
            let (module, offset) = (call.read_u64(index), call.read_u64(index + 8));
            let address = call.tls_block(module) + offset;
            call.set_return(address);
            Ok(())
        })
}

fn machine() -> Machine {
    let bytes = guest::build("program", &[("program.cpp", &[]), ("tls_initial_exec.cpp", &["-ftls-model=initial-exec"]), ("jump.S", &[])]);
    let images = load_images("program.so", &bytes, &mut |_| Ok(None)).unwrap();
    let options = TranslateOptions { shared_memory: false, ..TranslateOptions::default() };
    let output = translate(Program::bind(images).unwrap(), &options).unwrap();
    assert!(output.report.unresolved_jumps.is_empty(), "unresolved jumps: {:x?}", output.report.unresolved_jumps);
    let mut machine = Machine::new(&output.wasm, nids());
    machine.initialize().unwrap();
    machine
}

fn classify(value: i64) -> i64 {
    match value {
        0 => 3,
        1 => 14,
        2 => 15,
        3 => 92,
        4 => 65,
        5 => 35,
        6 => 89,
        7 => 79,
        _ => -1,
    }
}

#[test]
fn constructors_run_before_the_program() {
    let mut machine = machine();
    assert_eq!(machine.call_export(0, "run_initialized", &[]).unwrap(), 1000);
}

#[test]
fn switch_jump_table_selects_each_case() {
    let mut machine = machine();
    let expected = (-1..9).fold(0i64, |sum, value| sum * 3 + classify(value));
    assert_eq!(machine.call_export(0, "run_switch", &[]).unwrap() as i64, expected);
}

#[test]
fn imports_receive_integer_and_float_arguments() {
    let mut machine = machine();
    assert_eq!(machine.call_export(0, "run_imports", &[4]).unwrap(), 9 + 6);
    assert_eq!(machine.output(), &["4".to_string()]);
}

#[test]
fn imports_call_back_into_guest_code() {
    let mut machine = machine();
    assert_eq!(machine.call_export(0, "run_callback", &[5]).unwrap(), 15);
    assert_eq!(machine.call_export(0, "run_callback", &[4]).unwrap(), 8);
}

#[test]
fn indirect_calls_reach_function_pointers() {
    let mut machine = machine();
    assert_eq!(machine.call_export(0, "run_vtable", &[]).unwrap(), 12 + 15 + 4);
}

#[test]
fn thread_local_variables_keep_their_values() {
    let mut machine = machine();
    assert_eq!(machine.call_export(0, "run_tls", &[]).unwrap(), 609);
    assert_eq!(machine.call_export(0, "run_tls", &[]).unwrap(), 711);
}

#[test]
fn import_can_resume_an_outer_frame() {
    let mut machine = machine();
    assert_eq!(machine.call_export(0, "run_nonlocal", &[]).unwrap(), 77);
    assert!(machine.output().is_empty(), "code after the non-local transfer ran: {:?}", machine.output());
}

fn exception_nids() -> NidTable {
    NidTable::default()
        .with("host_print_long", |call| {
            let value = call.arg(0) as i64;
            call.output(value.to_string());
            Ok(())
        })
        .with("__cxa_allocate_exception", |call| {
            let size = call.arg(0);
            let object = call.allocate(size);
            call.set_return(object);
            Ok(())
        })
        .with("__cxa_throw", |call| {
            let object = call.arg(0);
            call.throw(object)
        })
        .with("_Unwind_Resume", |call| {
            let object = call.arg(0);
            call.throw(object)
        })
        .with("__cxa_begin_catch", |call| {
            let object = call.arg(0);
            call.set_return(object);
            Ok(())
        })
        .with("__cxa_end_catch", |_| Ok(()))
}

#[test]
fn exceptions_run_cleanups_and_reach_the_handler() {
    let bytes = guest::build("exceptions", &[("exceptions.cpp", &["-fexceptions"])]);
    let images = load_images("exceptions.so", &bytes, &mut |_| Ok(None)).unwrap();
    let options = TranslateOptions { shared_memory: false, ..TranslateOptions::default() };
    let output = translate(Program::bind(images).unwrap(), &options).unwrap();
    let mut machine = Machine::new(&output.wasm, exception_nids());
    machine.initialize().unwrap();
    assert_eq!(machine.call_export(0, "run_exceptions", &[5]).unwrap(), 101);
    assert_eq!(machine.call_export(0, "run_exceptions", &[0]).unwrap(), 201);
}

#[test]
fn guest_longjmp_resumes_the_setjmp_return_site() {
    let mut machine = machine();
    assert_eq!(machine.call_export(0, "run_guest_longjmp", &[]).unwrap(), 33 * 10 + 2);
    assert!(machine.output().is_empty(), "code after longjmp ran: {:?}", machine.output());
}

#[test]
fn init_code_in_a_writable_executable_segment_runs() {
    let bytes = guest::build_linked("writable_init", &[("writable_init.S", &[])], &["-init=writable_init"]);
    let images = load_images("writable_init.so", &bytes, &mut |_| Ok(None)).unwrap();
    let options = TranslateOptions { shared_memory: false, ..TranslateOptions::default() };
    let output = translate(Program::bind(images).unwrap(), &options).unwrap();
    let mut machine = Machine::new(&output.wasm, NidTable::default());
    machine.initialize().unwrap();
    assert_eq!(machine.call_export(0, "read_init_marker", &[]).unwrap(), 4242);
}
