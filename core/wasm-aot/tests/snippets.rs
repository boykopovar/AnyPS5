mod common;

use common::*;
use iced_x86::code_asm::*;

#[test]
fn mov_immediate_reaches_rax() {
    let code = assemble(|a| {
        a.mov(rax, 42u64)?;
        a.ret()
    });
    let state = run_wasm(&code, &State::default());
    assert_eq!(state.gpr[RAX], 42);
}
